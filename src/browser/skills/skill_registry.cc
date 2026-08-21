// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#include "chrome/browser/flux/skills/skill_registry.h"

#include <algorithm>

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"

namespace flux {
namespace {

mojom::WriteScope ParseScope(const std::string& value) {
  if (value == "draft")    return mojom::WriteScope::kDraft;
  if (value == "send")     return mojom::WriteScope::kSend;
  if (value == "purchase") return mojom::WriteScope::kPurchase;
  return mojom::WriteScope::kReadOnly;
}

int ScopeRank(mojom::WriteScope scope) {
  switch (scope) {
    case mojom::WriteScope::kReadOnly: return 0;
    case mojom::WriteScope::kDraft:    return 1;
    case mojom::WriteScope::kSend:     return 2;
    case mojom::WriteScope::kPurchase: return 3;
  }
  return 3;
}

// Strips a "[a, b]" list, or returns a single-element list.
std::vector<std::string> ParseList(std::string value) {
  base::TrimString(value, " ", &value);
  if (value.size() >= 2 && value.front() == '[' && value.back() == ']')
    value = value.substr(1, value.size() - 2);
  std::vector<std::string> out = base::SplitString(
      value, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  return out;
}

// Words too common to carry signal when matching a task to a skill.
bool IsStopWord(const std::string& w) {
  static constexpr const char* kStop[] = {
      "the", "a", "an", "and", "or", "for", "to", "of", "in", "on", "my",
      "me", "i", "with", "from", "into", "this", "that", "every", "all",
      "is", "are", "be", "it", "your", "you", "get", "make", "do",
  };
  for (const char* s : kStop) {
    if (w == s)
      return true;
  }
  return w.size() < 3;
}

std::vector<std::string> Tokenize(const std::string& text) {
  std::vector<std::string> out;
  for (std::string& w : base::SplitString(base::ToLowerASCII(text),
                                          " \t\n.,;:!?\"'()[]{}/-",
                                          base::TRIM_WHITESPACE,
                                          base::SPLIT_WANT_NONEMPTY)) {
    if (!IsStopWord(w))
      out.push_back(std::move(w));
  }
  return out;
}

}  // namespace

SkillRegistry::SkillRegistry(Profile* profile) : profile_(profile) {
  base::FilePath resources;
  if (base::PathService::Get(chrome::DIR_RESOURCES, &resources))
    LoadFromDirectory(resources.AppendASCII("flux_skills"), false);

  // User skills load second so a matching command shadows the shipped one.
  if (profile_) {
    LoadFromDirectory(profile_->GetPath().AppendASCII("Flux Skills"), true);
  }
}

SkillRegistry::~SkillRegistry() = default;

void SkillRegistry::LoadFromDirectory(const base::FilePath& dir,
                                      bool user_authored) {
  if (!base::DirectoryExists(dir))
    return;

  base::FileEnumerator files(dir, /*recursive=*/false,
                             base::FileEnumerator::FILES,
                             FILE_PATH_LITERAL("*.md"));
  for (base::FilePath path = files.Next(); !path.empty(); path = files.Next()) {
    std::string contents;
    if (!base::ReadFileToString(path, &contents))
      continue;
    std::optional<Skill> skill = ParseSkillFile(contents);
    if (!skill || skill->command.empty())
      continue;
    skill->user_authored = user_authored;
    skills_[skill->command] = std::move(*skill);
  }
}

// static
std::optional<Skill> SkillRegistry::ParseSkillFile(const std::string& contents) {
  // Frontmatter is delimited by --- lines; everything after the second is body.
  if (!base::StartsWith(contents, "---"))
    return std::nullopt;
  const size_t end = contents.find("\n---", 3);
  if (end == std::string::npos)
    return std::nullopt;

  Skill skill;
  const std::string frontmatter = contents.substr(4, end - 4);
  skill.body = contents.substr(contents.find('\n', end + 1) + 1);

  for (const std::string& raw : base::SplitString(
           frontmatter, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
    // Nested keys (worksWith entries, limits) are indented; the flat metadata
    // this needs is all at column zero.
    if (raw.empty() || raw[0] == ' ' || raw[0] == '-')
      continue;
    const size_t colon = raw.find(':');
    if (colon == std::string::npos)
      continue;

    const std::string key = raw.substr(0, colon);
    std::string value = raw.substr(colon + 1);
    // Trailing comments are used throughout the catalog for provenance notes.
    const size_t hash = value.find('#');
    if (hash != std::string::npos)
      value = value.substr(0, hash);
    base::TrimWhitespaceASCII(value, base::TRIM_ALL, &value);

    if (key == "command")           skill.command = value;
    else if (key == "name")         skill.name = value;
    else if (key == "description")  skill.description = value;
    else if (key == "categories")   skill.categories = ParseList(value);
    else if (key == "roles")        skill.roles = ParseList(value);
    else if (key == "writeScope")   skill.write_scope = ParseScope(value);
  }
  return skill;
}

const Skill* SkillRegistry::GetByCommand(const std::string& command) const {
  auto it = skills_.find(command);
  return it == skills_.end() ? nullptr : &it->second;
}

std::vector<const Skill*> SkillRegistry::FindRelevant(const std::string& task,
                                                      size_t limit) const {
  const std::vector<std::string> needles = Tokenize(task);
  if (needles.empty())
    return {};

  std::vector<std::pair<int, const Skill*>> scored;
  for (const auto& [command, skill] : skills_) {
    const std::vector<std::string> haystack =
        Tokenize(skill.name + " " + skill.description);
    int score = 0;
    for (const std::string& needle : needles) {
      if (std::find(haystack.begin(), haystack.end(), needle) != haystack.end())
        score += 2;
    }
    // Require more than one incidental word in common. A single shared token
    // is noise, and a wrongly-attached skill quietly steers the whole run.
    if (score >= 4)
      scored.emplace_back(score, &skill);
  }

  std::sort(scored.begin(), scored.end(),
            [](const auto& a, const auto& b) { return a.first > b.first; });

  std::vector<const Skill*> out;
  for (const auto& [score, skill] : scored) {
    if (out.size() >= limit)
      break;
    out.push_back(skill);
  }
  return out;
}

std::vector<const Skill*> SkillRegistry::ForScope(
    mojom::WriteScope scope) const {
  std::vector<const Skill*> out;
  for (const auto& [command, skill] : skills_) {
    if (ScopeRank(skill.write_scope) <= ScopeRank(scope))
      out.push_back(&skill);
  }
  return out;
}

}  // namespace flux
