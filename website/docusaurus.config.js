/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *
 * @format
 */

const {fbContent, fbInternalOnly} = require('internaldocs-fb-helpers');
const repoUrl = 'https://github.com/facebookresearch/vrs';

// With JSDoc @type annotations, IDEs can provide config autocompletion
/** @type {import('@docusaurus/types').DocusaurusConfig} */
(module.exports = {
  title: 'VRS',
  tagline: 'A file format designed to record & playback streams of AR/VR sensor data.',
  url: 'https://vrs.github.io',
  baseUrl: '/',
  onBrokenLinks: 'throw',
  onBrokenMarkdownLinks: 'warn',
  favicon: 'img/VRS-Icon.svg',
  organizationName: 'facebookresearch',
  projectName: 'vrs',

  presets: [
    [
      require.resolve('docusaurus-plugin-internaldocs-fb/docusaurus-preset'),
      /** @type {import('@docusaurus/preset-classic').Options} */
      ({
        docs: {
          sidebarPath: require.resolve('./sidebars.js'),
            editUrl: fbContent({
              internal:
                'https://www.internalfb.com/intern/diffusion/FBS/browse/master/arvr/libraries/vrs/website/',
              external:
                'https://github.com/facebookresearch/vrs/edit/main/website',
            }),
        },
        theme: {
          customCss: require.resolve('./src/css/custom.css'),
        },
        staticDocsProject: 'vrs',
        trackingFile: 'xplat/staticdocs/WATCHED_FILES',
        'remark-code-snippets': {
          baseDir: '..',
        },
        enableEditor: true,
      }),
    ],
  ],

  customFields: {
    fbRepoName: 'fbsource',
    ossRepoPath: 'arvr/libraries/vrs',
  },

  themeConfig:
    /** @type {import('@docusaurus/preset-classic').ThemeConfig} */
    ({
      navbar: {
        title: 'VRS',
        logo: {
          alt: 'VRS Logo',
          src: 'img/VRS-Icon.svg',
        },
        items: [
          {
            type: 'doc',
            docId: 'Overview',
            position: 'left',
            label: 'Documentation',
          },
          {
            href: 'https://github.com/facebookresearch/vrs#getting-started',
            position: 'left',
            label: 'Build Instructions',
          },
          // Please keep GitHub link to the right for consistency.
          {
            href: 'https://github.com/facebookresearch/vrs',
            label: 'GitHub',
            position: 'right',
          },
        ],
      },
      footer: {
        style: 'dark',
        links: [
          {
            title: 'Learn',
            items: [
              {
                label: 'Documentation',
                to: 'docs/Overview',
              },
            ],
          },
          {
            title: 'Community',
            items: [
              {
                label: 'Stack Overflow',
                href: 'https://stackoverflow.com/questions/tagged/vrs',
              },
              {
                label: 'Discord',
                href: 'https://discordapp.com/invite/vrs',
              },
            ],
          },
          {
            title: 'Legal',
            // Please do not remove the privacy and terms, it's a legal requirement.
            items: [
              {
                label: 'Privacy',
                href: 'https://opensource.facebook.com/legal/privacy/',
              },
              {
                label: 'Terms',
                href: 'https://opensource.facebook.com/legal/terms/',
              },
            ],
          },
          {
            title: 'Legal (continued)',
            // Please do not remove the privacy and terms, it's a legal requirement.
            items: [
              {
                label: 'Data Policy',
                href: 'https://opensource.facebook.com/legal/data-policy/',
              },
              {
                label: 'Cookie Policy',
                href: 'https://opensource.facebook.com/legal/cookie-policy/',
              },
            ],
          },
        ],
        logo: {
          alt: 'Facebook Open Source Logo',
          src: 'img/oss_logo.png',
          href: 'https://opensource.facebook.com',
        },
        // Please do not remove the credits, help to publicize Docusaurus :)
        copyright: `Copyright © ${new Date().getFullYear()} Facebook, Inc. Built with Docusaurus.`,
      },
    }),
});
