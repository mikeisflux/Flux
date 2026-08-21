// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#include "chrome/browser/flux/providers/provider_keys.h"

#include <utility>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/flux/flux_agent_service.h"
#include "chrome/browser/flux/flux_agent_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/os_crypt/async/browser/os_crypt_async.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "components/prefs/pref_service.h"

namespace flux {
namespace {

constexpr char kApiKeyPrefPrefix[] = "flux.api_key.";

std::string PrefNameFor(const std::string& provider) {
  return base::StrCat({kApiKeyPrefPrefix, provider});
}

}  // namespace

ApiKeyStore::ApiKeyStore(Profile* profile) : profile_(profile) {
  if (!g_browser_process || !g_browser_process->os_crypt_async())
    return;
  g_browser_process->os_crypt_async()->GetInstance(base::BindOnce(
      &ApiKeyStore::OnEncryptorReady, weak_factory_.GetWeakPtr()));
}

ApiKeyStore::~ApiKeyStore() = default;

void ApiKeyStore::OnEncryptorReady(
    scoped_refptr<os_crypt_async::Encryptor> encryptor) {
  encryptor_ = std::move(encryptor);
}

std::string ApiKeyStore::Get(const std::string& provider) const {
  if (!ready() || !profile_)
    return std::string();

  const std::string encoded =
      profile_->GetPrefs()->GetString(PrefNameFor(provider));
  if (encoded.empty())
    return std::string();

  std::string ciphertext;
  if (!base::Base64Decode(encoded, &ciphertext))
    return std::string();

  std::string plaintext;
  if (!encryptor_->DecryptString(ciphertext, &plaintext))
    return std::string();
  return plaintext;
}

bool ApiKeyStore::Set(const std::string& provider, const std::string& key) {
  if (!ready() || !profile_)
    return false;

  if (key.empty()) {
    Clear(provider);
    return true;
  }

  std::string ciphertext;
  if (!encryptor_->EncryptString(key, &ciphertext))
    return false;

  profile_->GetPrefs()->SetString(PrefNameFor(provider),
                                  base::Base64Encode(ciphertext));
  return true;
}

void ApiKeyStore::Clear(const std::string& provider) {
  if (profile_)
    profile_->GetPrefs()->ClearPref(PrefNameFor(provider));
}

std::string GetApiKey(Profile* profile, const std::string& provider) {
  if (!profile)
    return std::string();
  FluxAgentService* service = FluxAgentServiceFactory::GetForProfile(profile);
  return service && service->keys() ? service->keys()->Get(provider)
                                    : std::string();
}

void SetApiKey(Profile* profile,
               const std::string& provider,
               const std::string& key) {
  if (!profile)
    return;
  FluxAgentService* service = FluxAgentServiceFactory::GetForProfile(profile);
  if (service && service->keys())
    service->keys()->Set(provider, key);
}

}  // namespace flux
