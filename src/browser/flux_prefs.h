// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#ifndef CHROME_BROWSER_FLUX_FLUX_PREFS_H_
#define CHROME_BROWSER_FLUX_FLUX_PREFS_H_

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace flux::prefs {

// NONE of these are syncable, including the ones that arguably should be.
// Chromium DCHECKs that every SYNCABLE_PREF appears in its own central
// allowlist, and a pref that is not there is fatal at profile creation - it
// took the browser down before it drew a window. See RegisterProfilePrefs.

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

// Encrypted secrets, each a dictionary of name -> base64 ciphertext.
//
// A dictionary rather than a pref per secret, and the reason is load-bearing:
// PrefService::GetString on an unregistered path is a hard CHECK that takes
// the browser process down ("Trying to access an unregistered pref"), and a
// write to one is silently dropped. A pref name that embeds a provider or a
// connector id cannot be registered up front, so the id has to be a key inside
// a registered dictionary instead. See SecretStore.
//
// None of these are syncable. A secret that follows a profile onto another
// machine is a secret that leaks.
inline constexpr char kApiKeys[] = "flux.api_keys";
inline constexpr char kConnectorTokens[] = "flux.connector_tokens";

// The OAuth apps the user registered with each provider: client id, secret and
// redirect URI. Flux ships no client secrets - a secret inside a binary anyone
// can download is not a secret, and several of these providers say so
// themselves - so every connector is the user's own registration.
inline constexpr char kConnectorClients[] = "flux.connector_clients";

// Saved workflows, keyed by id, as {command, name, description, cron, prompt,
// template_id, write_scope, model, profile_id, credit_budget, enabled,
// last_run, last_fire_missed}.
//
// The scheduler holds these in memory and fires them; this is where they live
// between runs. Without it a workflow saved at 5pm is gone by morning, which
// is the one thing a scheduled task must not be - and the failure is silent,
// because an empty list looks exactly like a user who has not made one yet.
inline constexpr char kWorkflows[] = "flux.workflows";

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

}  // namespace flux::prefs

#endif  // CHROME_BROWSER_FLUX_FLUX_PREFS_H_
