// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#include "chrome/browser/flux/flux_prefs.h"

#include "components/pref_registry/pref_registry_syncable.h"

namespace flux::prefs {

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  // NOT registered SYNCABLE_PREF, and this is not a preference - it is a hard
  // requirement of Chromium's sync layer that took a startup crash to find.
  //
  // PrefModelAssociator::RegisterPref DCHECKs that every syncable pref appears
  // in Chromium's own central allowlist (the SyncablePrefsDatabase). A pref
  // registered SYNCABLE_PREF without an entry there is fatal at profile
  // creation:
  //
  //   FATAL: pref_model_associator.cc:459] DCHECK failed: Preference
  //   flux.adopted_skills has not been added to syncable prefs allowlist
  //
  // dcheck_always_on is true in dev.gn, so this took the browser down before
  // it ever drew a window - the browser process died, the GPU process came up
  // and went away again, and the whole thing exited 0 with no error on screen.
  //
  // Making them syncable properly means patching that allowlist, which is a
  // Chromium file to rebase on every uprev. It would also buy nothing today:
  // Chromium sync needs Google API keys, and dev.gn says outright that without
  // them "sync, safe-browsing, geolocation and translate are inert". So these
  // four were never going to follow a user to another machine - the intent was
  // real, the mechanism was not.
  //
  // Instructions, learned facts and the user's skill set are therefore local
  // to a profile for now. If sync is ever wanted, it needs the allowlist patch
  // AND working API keys, in that order, and this comment is the note saying
  // so.
  registry->RegisterStringPref(kInstructions, std::string());
  registry->RegisterListPref(kLearnedFacts);
  registry->RegisterListPref(kAdoptedSkills);
  registry->RegisterDictionaryPref(kUserSkills);

  // How wide a window is set up is a property of the screen in front of you,
  // not of the account.
  registry->RegisterBooleanPref(kSidebarCollapsed, false);

  // Encrypted secrets. Registered here and nowhere else - these three names
  // are the only pref paths SecretStore ever touches, which is what makes the
  // dictionary-of-secrets shape safe. Never syncable, on their own merits: a
  // secret that follows a profile onto another machine is a secret that leaks.
  registry->RegisterDictionaryPref(kApiKeys);
  registry->RegisterDictionaryPref(kConnectorTokens);
  registry->RegisterDictionaryPref(kConnectorClients);

  // Saved workflows. Not syncable: a schedule that fires on two machines runs
  // the task twice, and the second one has no way to know.
  registry->RegisterDictionaryPref(kWorkflows);
}

}  // namespace flux::prefs
