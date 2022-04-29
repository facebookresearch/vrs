/**
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *
 * @format
 */

import React from 'react';
import clsx from 'clsx';
import Layout from '@theme/Layout';
import Link from '@docusaurus/Link';
import useDocusaurusContext from '@docusaurus/useDocusaurusContext';
import useBaseUrl from '@docusaurus/useBaseUrl';
import styles from './styles.module.css';
import useThemeContext from '@theme/hooks/useThemeContext';

const features = [
  {
    title: 'Optimized Recording',
    //imageUrl: 'img/VRS-Icon.svg',
    description: (
      <>
        A very efficient data collection file format, so sensor data can be recorded in realtime on device.
      </>
    ),
  },
  {
    title: 'Powerful Data Format',
    //imageUrl: 'img/VRS-Icon.svg',
    description: (
      <>
		While most space efficient, file format changes don&#x27;t break compatibility, with lossless compression.
      </>
    ),
  },
  {
    title: 'Flexible',
    //imageUrl: 'img/VRS-Icon.svg',
    description: (
      <>
		Store metadata, images, audio and more.
      </>
    ),
  },
];

function Feature({imageUrl, title, description}) {
  const imgUrl = useBaseUrl(imageUrl);
  return (
    <div className={clsx('col col--4', styles.feature)}>
      {imgUrl && (
        <div className="text--center">
          <img className={styles.featureImage} src={imgUrl} alt={title} />
        </div>
      )}
      <h3>{title}</h3>
      <p>{description}</p>
    </div>
  );
}

function LogoImage() {
  const {isDarkTheme} = useThemeContext();
  const logoWhite = useBaseUrl('img/VRS-Logo.svg');
  const logo = useBaseUrl('img/VRS-Logo-DarkMode.svg');
  return (
    <img
      className={styles.heroLogo}
      src={isDarkTheme ? logoWhite : logo}
      alt="VRS Logo"
      width="600"
    />
  );
}

export default function Home() {
  const context = useDocusaurusContext();
  const {siteConfig = {}} = context;
  return (
    <Layout
      title={`VRS | Sensor Data File Format`}
      description="A file format designed to record & playback streams of XR sensor data">
      <header className={clsx('hero hero--primary', styles.heroBanner)}>
        <div className="container">
          <LogoImage />
          <p className="hero__subtitle">{siteConfig.tagline}</p>
          <div className={styles.buttons}>
            <Link
              className={clsx(
                'button button--outline button--secondary button--lg',
                styles.getStarted,
              )}
              to={useBaseUrl('docs/Overview')}>
              Get Started
            </Link>
          </div>
        </div>
      </header>
      <main>
        {features && features.length > 0 && (
          <section className={styles.features}>
            <div className="container">
              <div className="row">
                {features.map(({title, imageUrl, description}) => (
                  <Feature
                    key={title}
                    title={title}
                    imageUrl={imageUrl}
                    description={description}
                  />
                ))}
              </div>
            </div>
          </section>
        )}
      </main>
    </Layout>
  );
}
