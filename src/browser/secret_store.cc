// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#include "chrome/browser/flux/secret_store.h"

#include <utility>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "components/os_crypt/async/browser/os_crypt_async.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/pref_service.h"

namespace flux {

SecretStore::SecretStore(Profile* profile, std::string pref_name)
    : profile_(profile), pref_name_(std::move(pref_name)) {
  if (!g_browser_process || !g_browser_process->os_crypt_async())
    return;
  g_browser_process->os_crypt_async()->GetInstance(base::BindOnce(
      &SecretStore::OnEncryptorReady, weak_factory_.GetWeakPtr()));
}

SecretStore::~SecretStore() = default;

void SecretStore::OnEncryptorReady(
    scoped_refptr<os_crypt_async::Encryptor> encryptor) {
  encryptor_ = std::move(encryptor);
}

std::string SecretStore::Get(const std::string& name) const {
  if (!ready() || !profile_)
    return std::string();

  const base::DictValue& dict = profile_->GetPrefs()->GetDict(pref_name_);
  const std::string* encoded = dict.FindString(name);
  if (!encoded || encoded->empty())
    return std::string();

  std::string ciphertext;
  if (!base::Base64Decode(*encoded, &ciphertext))
    return std::string();

  std::string plaintext;
  if (!encryptor_->DecryptString(ciphertext, &plaintext))
    return std::string();
  return plaintext;
}

bool SecretStore::Set(const std::string& name, const std::string& value) {
  if (!ready() || !profile_)
    return false;

  if (value.empty()) {
    Clear(name);
    return true;
  }

  std::string ciphertext;
  if (!encryptor_->EncryptString(value, &ciphertext))
    return false;

  ScopedDictPrefUpdate update(profile_->GetPrefs(), pref_name_);
  update->Set(name, base::Base64Encode(ciphertext));
  return true;
}

void SecretStore::Clear(const std::string& name) {
  if (!profile_)
    return;
  ScopedDictPrefUpdate update(profile_->GetPrefs(), pref_name_);
  update->Remove(name);
}

std::vector<std::string> SecretStore::Names() const {
  std::vector<std::string> out;
  if (!profile_)
    return out;
  for (const auto [name, value] : profile_->GetPrefs()->GetDict(pref_name_))
    out.push_back(name);
  return out;
}

bool SecretStore::Has(const std::string& name) const {
  if (!profile_)
    return false;
  const std::string* encoded =
      profile_->GetPrefs()->GetDict(pref_name_).FindString(name);
  return encoded && !encoded->empty();
}

}  // namespace flux
