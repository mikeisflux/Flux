// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#ifndef CHROME_BROWSER_FLUX_SKILLS_SKILL_REGISTRY_H_
#define CHROME_BROWSER_FLUX_SKILLS_SKILL_REGISTRY_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/flux/mojom/flux.mojom.h"

class Profile;

namespace flux {

// One skill as loaded from disk.
//
// The schema mirrors the file format in data/skills/: YAML frontmatter for
// metadata, markdown body. Only `when_to_use` is structurally required - the
// rest of the body is freeform, because a charting skill wants a decision
// table and a research skill wants an interview script, and forcing both into
// the same template produces filler.
struct Skill {
  std::string command;      // "/social-post-facebook"
  std::string name;
  std::string description;  // also the retrieval trigger
  std::vector<std::string> categories;
  std::vector<std::string> roles;
  std::vector<std::string> connectors;
  mojom::WriteScope write_scope = mojom::WriteScope::kReadOnly;
  std::string body;         // full markdown, injected when the skill fires
  bool user_authored = false;
};

// Loads and serves the skill library.
//
// Skills are read from the bundled catalog and from the profile directory, so
// a user-authored skill sits alongside the shipped ones and can shadow one by
// reusing its command.
class SkillRegistry {
 public:
  explicit SkillRegistry(Profile* profile);
  ~SkillRegistry();

  SkillRegistry(const SkillRegistry&) = delete;
  SkillRegistry& operator=(const SkillRegistry&) = delete;

  // Exact lookup for an explicitly invoked command.
  const Skill* GetByCommand(const std::string& command) const;

  // Skills whose description plausibly matches `task`, best first.
  //
  // Deliberately lexical rather than embedding-based: the description text is
  // hand-written to be a trigger, the library is small enough that scanning it
  // is free, and a wrong skill silently steering a run is worse than no skill.
  // Callers should treat the result as candidates, not as a decision.
  std::vector<const Skill*> FindRelevant(const std::string& task,
                                         size_t limit) const;

  // Skills whose write scope is within `scope`. A read-only task should never
  // be handed a skill whose instructions assume it can send.
  std::vector<const Skill*> ForScope(mojom::WriteScope scope) const;

  size_t size() const { return skills_.size(); }

 private:
  void LoadFromDirectory(const base::FilePath& dir, bool user_authored);
  static std::optional<Skill> ParseSkillFile(const std::string& contents);

  raw_ptr<Profile> profile_;
  std::map<std::string, Skill> skills_;   // keyed by command
};

}  // namespace flux

#endif  // CHROME_BROWSER_FLUX_SKILLS_SKILL_REGISTRY_H_
