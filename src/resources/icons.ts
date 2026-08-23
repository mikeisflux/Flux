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
