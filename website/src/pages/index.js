/**
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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
import {useColorMode} from '@docusaurus/theme-common';

const features = [
  {
    title: 'Optimized Recording',
    description: (
      <>
        Multi-Stream<div/>
        Thread-Safe<div/>
        Lossless Compression<div/>
      </>
    ),
  },
  {
    title: 'Resilient Data Format',
    description: (
      <>
        Self-Described<div/>
        Compact Binary Representation<div/>
		    Change Resilient<div/>
      </>
    ),
  },
  {
    title: 'Flexible',
    description: (
      <>
        Rich Metadata<div/>
		    Images (raw or compressed)<div/>
        Audio<div/>
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
  const {colorMode} = useColorMode();
  const logoBlackText = useBaseUrl('img/VRS-Logo.svg');
  const logoWhiteText = useBaseUrl('img/VRS-Logo-DarkMode.svg');
  return (
    <img
      className={styles.vrsBanner}
      src={colorMode === 'dark' ? logoBlackText : logoWhiteText}
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
      <header className={clsx('hero hero--primary', styles.vrsBanner)}>
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
