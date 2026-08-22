// Mirrors ui/webui/resources/tools/stylelint.config_base.mjs at the pinned
// Chromium tag, with the plugin resolved from node_modules instead of by
// relative path into third_party. Keep the rules in sync on every uprev - the
// build runs Chromium's copy, and this one exists so the same failures surface
// here first.

export default {
  plugins: ['@stylistic/stylelint-plugin'],
  rules: {
    'at-rule-no-unknown': true,
    'color-no-invalid-hex': true,
    'custom-property-no-missing-var-function': true,
    'declaration-block-no-duplicate-custom-properties': true,
    'declaration-block-no-duplicate-properties': true,
    'declaration-block-no-shorthand-property-overrides': true,
    'declaration-property-value-no-unknown': true,
    'function-calc-no-unspaced-operator': true,
    'function-no-unknown': true,
    'media-feature-name-no-unknown': true,
    'media-feature-name-value-no-unknown': true,
    'no-duplicate-selectors': true,
    'no-irregular-whitespace': true,
    'property-no-unknown': true,
    'selector-pseudo-class-no-unknown': true,
    'selector-pseudo-element-no-unknown': true,
    'unit-no-unknown': true,
    'block-no-empty': true,
    'color-hex-length': 'short',
    'length-zero-no-unit': [true, { "ignore": ["custom-properties"] }],
    'rule-empty-line-before': ['always', { 'ignore': ['after-comment', 'first-nested', 'inside-block'] }],
    '@stylistic/no-missing-end-of-source-newline': true,
    '@stylistic/string-quotes': 'single',
    '@stylistic/declaration-block-semicolon-newline-after': 'always',
    '@stylistic/declaration-block-semicolon-newline-before': 'never-multi-line',
    '@stylistic/declaration-block-semicolon-space-before': 'never',
    '@stylistic/declaration-block-trailing-semicolon': 'always',
    '@stylistic/no-extra-semicolons': true,
    '@stylistic/selector-list-comma-newline-after': 'always',
    '@stylistic/media-feature-colon-space-after': 'always',
    '@stylistic/media-feature-colon-space-before': 'never',
    '@stylistic/media-feature-range-operator-space-before': 'always',
    '@stylistic/media-feature-range-operator-space-after': 'always',
    '@stylistic/media-feature-parentheses-space-inside': 'never',
    '@stylistic/media-feature-name-case': 'lower',
  }
};
