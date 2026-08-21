// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

#include "chrome/browser/flux/webui/flux_ui.h"

#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/flux_resources.h"
#include "chrome/grit/flux_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/webui/webui_util.h"

namespace flux {

FluxUI::FluxUI(content::WebUI* web_ui) : ui::MojoWebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* source =
      content::WebUIDataSource::CreateAndAdd(profile, chrome::kChromeUIFluxHost);

  // Takes base::span<const ResourcePath>; the generated array converts
  // directly. base::make_span(ptr, size) is not the current form.
  webui::SetupWebUIDataSource(source, kFluxResources, IDR_FLUX_INDEX_HTML);

  // The console renders task titles, page titles, and model output - all of
  // which originate outside the browser. Keep the default WebUI CSP intact so
  // a malicious page title cannot execute in a privileged renderer.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources chrome://webui-test 'self';");

  // Screenshots captured during a run are rendered inline in the run view.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ImgSrc,
      "img-src chrome://resources chrome://image data: blob: 'self';");
}

FluxUI::~FluxUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(FluxUI)

void FluxUI::BindInterface(
    mojo::PendingReceiver<mojom::FluxPageHandlerFactory> receiver) {
  factory_receiver_.reset();
  factory_receiver_.Bind(std::move(receiver));
}

void FluxUI::CreatePageHandler(
    mojo::PendingRemote<mojom::FluxPageHandlerObserver> observer,
    mojo::PendingReceiver<mojom::FluxPageHandler> handler) {
  page_handler_ = std::make_unique<FluxPageHandler>(
      std::move(handler), std::move(observer),
      Profile::FromWebUI(web_ui()), web_ui()->GetWebContents());
}

}  // namespace flux
