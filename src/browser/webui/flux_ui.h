// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#ifndef CHROME_BROWSER_FLUX_WEBUI_FLUX_UI_H_
#define CHROME_BROWSER_FLUX_WEBUI_FLUX_UI_H_

#include <memory>

#include "chrome/browser/flux/mojom/flux.mojom.h"
#include "chrome/browser/flux/webui/flux_page_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace flux {


// Serves chrome://flux - the agent console (sidebar, templates, workflows,
// connectors, customize, and the run view).
//
// Running the console as WebUI rather than as a bundled web app is what lets
// it talk to the browser process over Mojo with no network hop, and keeps it
// out of reach of any page the agent visits.
class FluxUI : public ui::MojoWebUIController,
               public mojom::FluxPageHandlerFactory {
 public:
  explicit FluxUI(content::WebUI* web_ui);
  ~FluxUI() override;

  FluxUI(const FluxUI&) = delete;
  FluxUI& operator=(const FluxUI&) = delete;

  void BindInterface(
      mojo::PendingReceiver<mojom::FluxPageHandlerFactory> receiver);

 private:
  // mojom::FluxPageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<mojom::FluxPageHandlerObserver> observer,
      mojo::PendingReceiver<mojom::FluxPageHandler> handler) override;

  std::unique_ptr<FluxPageHandler> page_handler_;
  mojo::Receiver<mojom::FluxPageHandlerFactory> factory_receiver_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

// Declared after FluxUI so the DefaultWebUIConfig template sees a complete
// type - its CreateWebUIController static_asserts on constructibility.
class FluxUIConfig : public content::DefaultWebUIConfig<FluxUI> {
 public:
  FluxUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUIFluxHost) {}
};

}  // namespace flux

#endif  // CHROME_BROWSER_FLUX_WEBUI_FLUX_UI_H_
