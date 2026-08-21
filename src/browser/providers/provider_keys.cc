// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#include "chrome/browser/flux/providers/provider_keys.h"

#include "base/base64.h"
#include "base/strings/strcat.h"
#include "chrome/browser/profiles/profile.h"
#include "components/os_crypt/sync/os_crypt.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace flux {
namespace {

// Stored per-profile. Deliberately NOT registered as a syncable pref: a key
// that follows a profile onto another machine is a key that leaks.
constexpr char kApiKeyPrefPrefix[] = "flux.api_key.";

std::string PrefNameFor(const std::string& provider) {
  return base::StrCat({kApiKeyPrefPrefix, provider});
}

}  // namespace

std::string GetApiKey(Profile* profile, const std::string& provider) {
  if (!profile)
    return std::string();

  const std::string encoded =
      profile->GetPrefs()->GetString(PrefNameFor(provider));
  if (encoded.empty())
    return std::string();

  std::string ciphertext;
  if (!base::Base64Decode(encoded, &ciphertext))
    return std::string();

  std::string plaintext;
  // OSCrypt is the same mechanism Chromium uses for saved passwords: DPAPI on
  // Windows, Keychain on macOS, the platform secret service on Linux.
  if (!OSCrypt::DecryptString(ciphertext, &plaintext))
    return std::string();

  return plaintext;
}

void SetApiKey(Profile* profile,
               const std::string& provider,
               const std::string& key) {
  if (!profile)
    return;

  if (key.empty()) {
    profile->GetPrefs()->ClearPref(PrefNameFor(provider));
    return;
  }

  std::string ciphertext;
  if (!OSCrypt::EncryptString(key, &ciphertext)) {
    // Refuse to fall back to plaintext. A key that cannot be encrypted is not
    // stored at all - the user gets an error rather than a silent downgrade.
    return;
  }

  profile->GetPrefs()->SetString(PrefNameFor(provider),
                                 base::Base64Encode(ciphertext));
}

}  // namespace flux
