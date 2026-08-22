// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#include "chrome/browser/flux/flux_prefs.h"

#include "components/pref_registry/pref_registry_syncable.h"

namespace flux::prefs {

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  // Syncable: instructions and an adopted skill set are the user's own
  // configuration and should follow them to another machine. Learned facts
  // sync too - they are the same buffer conceptually, and a memory that only
  // exists on one desktop is a memory the user cannot trust.
  registry->RegisterStringPref(
      kInstructions, std::string(),
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterListPref(
      kLearnedFacts, user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterListPref(
      kAdoptedSkills, user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterDictionaryPref(
      kUserSkills, user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  // Not syncable: how wide a window is set up is a property of the screen in
  // front of you, not of the account.
  registry->RegisterBooleanPref(kSidebarCollapsed, false);

  // Encrypted secrets. Registered here and nowhere else - these three names
  // are the only pref paths SecretStore ever touches, which is what makes the
  // dictionary-of-secrets shape safe. Never syncable.
  registry->RegisterDictionaryPref(kApiKeys);
  registry->RegisterDictionaryPref(kConnectorTokens);
  registry->RegisterDictionaryPref(kConnectorClients);
}

}  // namespace flux::prefs
