// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#include "chrome/browser/flux/agent/tools/file_tools.h"

#include <memory>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/flux/agent/tool_registry.h"
#include "chrome/browser/flux/flux_agent_service.h"
#include "chrome/common/chrome_paths.h"
#include "net/base/filename_util.h"
#include "url/gurl.h"

namespace flux {
namespace {

ToolResult Ok(std::string content) {
  ToolResult r;
  r.content = std::move(content);
  return r;
}

ToolResult Err(std::string content) {
  ToolResult r;
  r.content = std::move(content);
  r.is_error = true;
  return r;
}

base::DictValue StringProperty(const std::string& description) {
  base::DictValue property;
  property.Set("type", "string");
  property.Set("description", description);
  return property;
}

// Anything the model supplies as a filename is untrusted. A name is reduced to
// its own last component and stripped of separators, so "../../.bashrc" and
// "C:\\Windows\\system32\\x" both land in the downloads directory as a plain
// file and cannot climb out of it.
base::FilePath SafeName(const std::string& raw) {
  std::string name = raw;
  for (char& c : name) {
    if (c == '/' || c == '\\' || c == ':' || c == '\0')
      c = '_';
  }
  base::TrimWhitespaceASCII(name, base::TRIM_ALL, &name);
  while (!name.empty() && name.front() == '.')
    name.erase(name.begin());
  if (name.empty())
    name = "flux-output.txt";
  if (name.size() > 120)
    name.resize(120);
#if BUILDFLAG(IS_WIN)
  return base::FilePath(base::UTF8ToWide(name));
#else
  return base::FilePath(name);
#endif
}

struct WriteResult {
  bool ok = false;
  base::FilePath path;
};

// Runs on a worker thread: file I/O never happens on the UI thread, and a
// multi-megabyte CSV would visibly hitch the browser if it did.
WriteResult WriteOnWorker(base::FilePath dir,
                          base::FilePath name,
                          std::string contents) {
  WriteResult result;
  if (dir.empty() || !base::CreateDirectory(dir))
    return result;
  // Never overwrite. Two runs producing "prospects.csv" should leave two
  // files, not one; a task silently replacing the output of the previous one
  // is the kind of loss nobody notices until it matters.
  base::FilePath path = base::GetUniquePath(dir.Append(name));
  if (path.empty())
    return result;
  if (!base::WriteFile(path, contents))
    return result;
  result.ok = true;
  result.path = std::move(path);
  return result;
}

class WriteFileTool : public Tool {
 public:
  explicit WriteFileTool(FluxAgentService* service) : service_(service) {}

  std::string name() const override { return "write_file"; }

  std::string description() const override {
    return "Save a file to the user's Downloads folder and show it to them as "
           "something they can open. Use it for the actual deliverable when it "
           "has nowhere else to live: a CSV of the rows you gathered, a "
           "report, a JSON export, anything the user would otherwise have to "
           "select out of the chat by hand. Prefer a real Google Sheet or Doc "
           "when the task asked for one - this is for output that has no URL. "
           "Do not use it for scratch data, and do not paste a thousand rows "
           "into your reply instead of calling this.";
  }

  mojom::WriteScope RequiredScope() const override {
    // It writes to the user's disk, which they should have agreed to. It
    // transmits nothing, so it stops short of kSend.
    return mojom::WriteScope::kDraft;
  }

  base::DictValue InputSchema() const override {
    base::DictValue properties;
    properties.Set("filename", StringProperty(
        "With an extension: \"acme-prospects.csv\". No path, no folders - it "
        "goes to Downloads."));
    properties.Set("contents", StringProperty("The whole file, as text."));
    properties.Set("title", StringProperty(
        "Optional. What to call it on the card, in the user's words rather "
        "than the filename."));

    base::ListValue required;
    required.Append("filename");
    required.Append("contents");

    base::DictValue schema;
    schema.Set("type", "object");
    schema.Set("properties", std::move(properties));
    schema.Set("required", std::move(required));
    return schema;
  }

  std::string DescribeEffect(const base::DictValue& input) const override {
    const std::string* name = input.FindString("filename");
    return base::StrCat({"Save ", name ? *name : "a file", " to Downloads"});
  }

  void Run(const ToolContext& context,
           base::DictValue input,
           ResultCallback callback) override {
    const std::string* filename = input.FindString("filename");
    const std::string* contents = input.FindString("contents");
    if (!filename || filename->empty()) {
      std::move(callback).Run(Err("write_file needs a filename."));
      return;
    }
    if (!contents) {
      std::move(callback).Run(Err("write_file needs contents."));
      return;
    }
    // A ceiling, because the whole file crosses the model boundary as a string
    // and a runaway generation should not fill the user's disk.
    constexpr size_t kMaxBytes = 8 * 1024 * 1024;
    if (contents->size() > kMaxBytes) {
      std::move(callback).Run(Err(base::StrCat(
          {"That file is ", base::NumberToString(contents->size() / 1024),
           " KB, over the ", base::NumberToString(kMaxBytes / 1024 / 1024),
           " MB limit. Write a summary and the first rows instead, or split "
           "it."})));
      return;
    }

    base::FilePath dir;
    if (!base::PathService::Get(chrome::DIR_DEFAULT_DOWNLOADS_SAFE, &dir)) {
      std::move(callback).Run(
          Err("Could not find the Downloads folder on this machine."));
      return;
    }

    const std::string* title = input.FindString("title");
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
        base::BindOnce(&WriteOnWorker, dir, SafeName(*filename), *contents),
        base::BindOnce(&WriteFileTool::OnWritten, weak_factory_.GetWeakPtr(),
                       context.run_id, title ? *title : *filename,
                       std::move(callback)));
  }

 private:
  void OnWritten(std::string run_id,
                 std::string title,
                 ResultCallback callback,
                 WriteResult result) {
    if (!result.ok) {
      std::move(callback).Run(
          Err("The file could not be written. The Downloads folder may be "
              "full or not writable."));
      return;
    }

    const GURL url = net::FilePathToFileURL(result.path);
    if (service_) {
      // Recorded as an artifact so it appears on the run's card with an Open
      // button, rather than only as a sentence in the closing message.
      auto artifact = mojom::RunArtifact::New();
      artifact->title = title;
      artifact->kind = "File";
      artifact->url = url;
      service_->AddArtifact(run_id, std::move(artifact));
    }

    std::move(callback).Run(Ok(base::StrCat(
        {"Saved to ", result.path.AsUTF8Unsafe(),
         ". The user can open it from the run - do not repeat its contents in "
         "your reply."})));
  }

  raw_ptr<FluxAgentService> service_;
  base::WeakPtrFactory<WriteFileTool> weak_factory_{this};
};

}  // namespace

void RegisterFileTools(ToolRegistry* registry, FluxAgentService* service) {
  registry->Register(std::make_unique<WriteFileTool>(service));
}

}  // namespace flux
