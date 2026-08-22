// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#ifndef CHROME_BROWSER_FLUX_FLUX_PREFS_H_
#define CHROME_BROWSER_FLUX_FLUX_PREFS_H_

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace flux::prefs {

// What the user typed under Customize > Instructions. Prepended to every task.
inline constexpr char kInstructions[] = "flux.instructions";

// What the agent has worked out about the user, as a list of
// {id, text, run_id, learned_at}. Deliberately a separate pref from the one
// above rather than appended to it: the reference lets the agent write into
// the same buffer the user edits, which means it can silently overwrite what
// the user wrote and there is no way to tell the two apart afterwards.
inline constexpr char kLearnedFacts[] = "flux.learned_facts";

// Commands of the skills the user has adopted. The shipped library is a
// catalog to adopt from; an empty list means the agent applies none of it
// automatically.
inline constexpr char kAdoptedSkills[] = "flux.adopted_skills";

// Skills the user wrote or edited, keyed by command, as
// {name, description, instructions}. An adopted skill that has been edited
// lives here and shadows the shipped one.
inline constexpr char kUserSkills[] = "flux.user_skills";

// Whether the console's column is collapsed to its icon rail. A pref rather
// than page state because the frame decides the window's layout and the page
// only draws inside it - both have to agree, and only one of them survives a
// restart.
inline constexpr char kSidebarCollapsed[] = "flux.sidebar_collapsed";

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

}  // namespace flux::prefs

#endif  // CHROME_BROWSER_FLUX_FLUX_PREFS_H_
