// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#ifndef CHROME_BROWSER_FLUX_FLUX_AGENT_SERVICE_FACTORY_H_
#define CHROME_BROWSER_FLUX_FLUX_AGENT_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace flux {

class FluxAgentService;

class FluxAgentServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static FluxAgentService* GetForProfile(Profile* profile);
  static FluxAgentServiceFactory* GetInstance();

  FluxAgentServiceFactory(const FluxAgentServiceFactory&) = delete;
  FluxAgentServiceFactory& operator=(const FluxAgentServiceFactory&) = delete;

 private:
  friend base::NoDestructor<FluxAgentServiceFactory>;

  FluxAgentServiceFactory();
  ~FluxAgentServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace flux

#endif  // CHROME_BROWSER_FLUX_FLUX_AGENT_SERVICE_FACTORY_H_
