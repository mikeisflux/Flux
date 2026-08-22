// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#ifndef CHROME_BROWSER_FLUX_WEBUI_FLUX_UI_H_
#define CHROME_BROWSER_FLUX_WEBUI_FLUX_UI_H_

#include <memory>
#include <string_view>

#include "chrome/browser/flux/mojom/flux.mojom.h"
#include "chrome/browser/flux/webui/flux_page_handler.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_config.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace flux {

class FluxUI;

// A TopChrome config rather than a plain one: WebUIContentsWrapperT looks the
// page up through TopChromeWebUIConfig::From() before it will host it in a
// side panel, and returns null for anything else.
class FluxUIConfig : public DefaultTopChromeWebUIConfig<FluxUI> {
 public:
  FluxUIConfig();
};

// Serves chrome://flux - the agent console (sidebar, templates, workflows,
// connectors, customize, and the run view).
//
// Running the console as WebUI rather than as a bundled web app is what lets
// it talk to the browser process over Mojo with no network hop, and keeps it
// out of reach of any page the agent visits.
class FluxUI : public TopChromeWebUIController,
               public mojom::FluxPageHandlerFactory {
 public:
  explicit FluxUI(content::WebUI* web_ui);

  // Required by WebUIContentsWrapperT; also names the renderer
  // process in the task manager.
  static constexpr std::string_view GetWebUIName() { return "Flux"; }

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

}  // namespace flux

#endif  // CHROME_BROWSER_FLUX_WEBUI_FLUX_UI_H_
