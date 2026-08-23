// Copyright 2026 Flux. Based on Chromium, Copyright The Chromium Authors.

/**
 * The console's inline SVG icons, built as DOM rather than parsed from markup.
 *
 * NOT innerHTML, and not a style preference. A WebUI data source turns on
 * Trusted Types - webui::SetupWebUIDataSource calls EnableTrustedTypesCSP,
 * which sets `require-trusted-types-for 'script'` - so assigning a string to
 * .innerHTML throws:
 *
 *   Failed to set the 'innerHTML' property on 'Element':
 *   This document requires 'TrustedHTML' assignment.
 *
 * Every screen in the console drew at least one icon that way, so every screen
 * threw partway through rendering. The exception surfaces as a failed screen,
 * which is honest but useless - the icon is not the point of the page.
 *
 * createElementNS builds the same nodes without going through the HTML parser,
 * so there is no string to trust and nothing to sanitise. It is also what
 * Chromium's own WebUIs do.
 */

import {BRAND_MARKS} from './connector_icons.js';

const SVG_NS = 'http://www.w3.org/2000/svg';

/** An empty <svg> with the given viewBox, hidden from assistive tech. */
export function svgRoot(viewBox: string, className?: string): SVGSVGElement {
  const root = document.createElementNS(SVG_NS, 'svg');
  root.setAttribute('viewBox', viewBox);
  root.setAttribute('aria-hidden', 'true');
  if (className) {
    root.setAttribute('class', className);
  }
  return root;
}

/** Appends one shape - path, circle, rect - to an <svg>. */
export function shape(
    root: SVGSVGElement, name: 'path'|'circle'|'rect',
    attributes: Record<string, string|number>): SVGElement {
  const node = document.createElementNS(SVG_NS, name);
  for (const [key, value] of Object.entries(attributes)) {
    node.setAttribute(key, String(value));
  }
  root.append(node);
  return node;
}

/** The common case: one path in a 20x20 box. */
export function pathIcon(
    d: string, viewBox = '0 0 20 20', className?: string): SVGSVGElement {
  const root = svgRoot(viewBox, className);
  shape(root, 'path', {d});
  return root;
}

/** The magnifier used by every search field in the console. */
export function searchIcon(): SVGSVGElement {
  const root = svgRoot('0 0 20 20');
  shape(root, 'circle', {cx: 9, cy: 9, r: 5.5});
  shape(root, 'path', {d: 'M13 13l4 4'});
  return root;
}

/**
 * A connector's brand mark, or its monogram when there is no mark to show.
 *
 * The marks are the only saturated colour in the console. That is deliberate
 * and it is why the rest of the UI has no accent: a person scanning a list of
 * tasks is asking "what does this touch", and a row of identical grey chips
 * cannot answer it.
 *
 * Eleven of the forty have no mark, because theirs is in no source this repo
 * can redistribute - several were pulled from Simple Icons at the trademark
 * holder's request, and tracing a replacement would be doing the thing they
 * asked not to be done. Those get a monogram, which is honest about being a
 * placeholder rather than an approximate logo. Nothing is matched on name
 * alone either: Apollo GraphQL's mark is not Apollo.io's, and a confidently
 * wrong logo is worse than a letter.
 *
 * A mark can be one path or several, in one colour or several - monday's is
 * three - so each is a list of shapes, built as nodes rather than markup
 * because Trusted Types blocks innerHTML on a WebUI page.
 */
export function connectorMark(
    id: string, className: string, monogram?: string): HTMLElement {
  const mark = document.createElement('span');
  mark.className = className;

  const brand = BRAND_MARKS[id];
  if (!brand) {
    mark.textContent = (monogram ?? id).slice(0, 1).toUpperCase();
    return mark;
  }

  mark.dataset['brand'] = '';
  const root = svgRoot(brand.viewBox);
  for (const {tag, attrs} of brand.shapes) {
    const node = document.createElementNS(SVG_NS, tag);
    for (const [key, value] of Object.entries(attrs)) {
      node.setAttribute(key, value);
    }
    root.append(node);
  }
  mark.append(root);
  return mark;
}
