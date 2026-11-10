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
 */

'use strict';

const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const os = require('node:os');
const http = require('node:http');
const {createRequire} = require('node:module');
const {test} = require('node:test');
const {once} = require('node:events');
const vm = require('node:vm');

const requireSite = createRequire(path.join(__dirname, '..', 'package.json'));

function consumerRequire(name) {
  return createRequire(requireSite.resolve(`${name}/package.json`));
}

test('AJV resolves relative schema references with the patched URI resolver', () => {
  const Ajv = consumerRequire('schema-utils')('ajv');
  const ajv = new Ajv({uriResolver: requireSite('fast-uri')});
  ajv.addSchema({
    $id: 'https://example.com/schemas/person.json',
    type: 'object',
    properties: {name: {type: 'string'}},
    required: ['name'],
    additionalProperties: false,
  });
  const validate = ajv.compile({
    $id: 'https://example.com/schemas/wrapper.json',
    $ref: './person.json',
  });
  assert.equal(validate({name: 'example'}), true);
  assert.equal(validate({name: 42}), false);
  assert.equal(validate({}), false);
});

test('webpack consumers preserve serializer semantics', async (t) => {
  for (const parent of [
    'copy-webpack-plugin',
    'css-minimizer-webpack-plugin',
  ]) {
    await t.test(parent, () => {
      const consumer = consumerRequire(parent);
      const fixture = {
        date: new Date('2020-01-01T00:00:00Z'),
        pattern: /example/gi,
        increment: function increment(value) {
          return value + 1;
        },
        text: '</script>',
      };
      const output = consumer('serialize-javascript')(fixture);
      assert.equal(output.includes('</script>'), false);
      const restored = vm.runInNewContext(`(${output})`, {Date, RegExp});
      assert.ok(restored.date instanceof Date);
      assert.equal(restored.date.toISOString(), fixture.date.toISOString());
      assert.ok(restored.pattern instanceof RegExp);
      assert.equal(restored.pattern.source, fixture.pattern.source);
      assert.equal(restored.pattern.flags, fixture.pattern.flags);
      assert.equal(restored.pattern.test('EXAMPLE'), true);
      assert.equal(restored.increment(41), 42);
      assert.equal(restored.text, fixture.text);
    });
  }
});

test('website SVG loader preserves the exact viewBox', async () => {
  const siteConfig = requireSite('./docusaurus.config.js');
  const plugin = requireSite('@docusaurus/plugin-svgr');
  const options = plugin.validateOptions({
    options: siteConfig.presets[0][1].svgr ?? {},
    validate(schema, value) {
      const result = schema.validate(value);
      assert.ifError(result.error);
      return result.value;
    },
  });
  const {getFileLoaderUtils} = requireSite('@docusaurus/utils');
  const config = {
    module: {rules: [getFileLoaderUtils(false).rules.svgs()]},
  };
  plugin.default({}, options).configureWebpack(config, false);
  const loader = config.module.rules[0].oneOf[0].use[0];
  const consumer = consumerRequire('@svgr/webpack');
  const {transform} = consumer('@svgr/core');
  const output = await transform(
    '<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 16 16"><path d="M0 0h16v16H0z"/></svg>',
    {
      ...loader.options,
      plugins: [
        consumer.resolve('@svgr/plugin-svgo'),
        consumer.resolve('@svgr/plugin-jsx'),
      ],
    },
    {componentName: 'SafeSvg'},
  );
  assert.match(output, /SafeSvg/);
  assert.match(output, /viewBox="0 0 16 16"/);
});

test('KaTeX resolved from Mermaid renders a formula server-side', () => {
  const consumer = consumerRequire('mermaid');
  const output = consumer('katex').renderToString('E=mc^2', {
    throwOnError: true,
  });
  assert.match(output, /katex/);
});

test(
  'SockJS echoes over a WebSocket connection',
  {timeout: 15000},
  async (t) => {
    const consumer = consumerRequire('sockjs');
    const uuid = consumer('uuid');
    assert.equal(uuid.validate(uuid.v4()), true);
    const server = http.createServer();
    const connections = new Set();
    server.on('connection', (connection) => {
      connections.add(connection);
      connection.on('close', () => connections.delete(connection));
    });
    let socket;
    t.after(async () => {
      socket?.close();
      for (const connection of connections) connection.destroy();
      await new Promise((resolve) => server.close(resolve));
    });
    const sockjs = requireSite('sockjs').createServer({log() {}});
    sockjs.on('connection', (connection) =>
      connection.on('data', (data) => connection.write(data)),
    );
    sockjs.installHandlers(server, {prefix: '/sockjs'});
    server.listen(0, '127.0.0.1');
    await once(server, 'listening', {signal: t.signal});
    const port = server.address().port;
    const response = await fetch(`http://127.0.0.1:${port}/sockjs/info`, {
      signal: t.signal,
    });
    assert.equal(response.status, 200);
    assert.equal((await response.json()).websocket, true);
    t.signal.throwIfAborted();
    socket = new WebSocket(
      `ws://127.0.0.1:${port}/sockjs/000/session/websocket`,
    );
    await new Promise((resolve, reject) => {
      let settled = false;
      const finish = (error) => {
        if (settled) return;
        settled = true;
        t.signal.removeEventListener('abort', abort);
        socket.onmessage = null;
        socket.onerror = null;
        socket.onclose = null;
        if (error) reject(error);
        else resolve();
      };
      const abort = () => finish(t.signal.reason);
      socket.onerror = () => finish(new Error('SockJS WebSocket error'));
      socket.onclose = () =>
        finish(new Error('SockJS WebSocket closed before echo'));
      socket.onmessage = (event) => {
        if (event.data === 'o') {
          socket.send('["hello"]');
        } else if (event.data === 'a["hello"]') {
          finish();
        }
      };
      t.signal.addEventListener('abort', abort, {once: true});
      if (t.signal.aborted) abort();
    });
  },
);

test(
  'webpack development server compiles and serves locally',
  {timeout: 30000},
  async (t) => {
    const webpack = requireSite('webpack');
    const Server = requireSite('webpack-dev-server');
    const directory = fs.mkdtempSync(
      path.join(os.tmpdir(), 'vrs-devserver-test-'),
    );
    let compiler;
    let server;
    t.after(async () => {
      try {
        if (server) await server.stop();
        else if (compiler) {
          await new Promise((resolve, reject) =>
            compiler.close((error) => (error ? reject(error) : resolve())),
          );
        }
      } finally {
        fs.rmSync(directory, {recursive: true, force: true});
      }
    });
    fs.writeFileSync(
      path.join(directory, 'entry.js'),
      'console.log("vrs-security-smoke");\n',
    );
    compiler = webpack({
      mode: 'development',
      context: directory,
      entry: './entry.js',
      output: {path: path.join(directory, 'dist'), filename: 'bundle.js'},
      infrastructureLogging: {level: 'none'},
    });
    server = new Server(
      {
        host: '127.0.0.1',
        port: 0,
        client: false,
        static: false,
        devMiddleware: {stats: false},
      },
      compiler,
    );
    t.signal.throwIfAborted();
    await server.start();
    const response = await fetch(
      `http://127.0.0.1:${server.server.address().port}/bundle.js`,
      {signal: t.signal},
    );
    assert.equal(response.status, 200);
    assert.match(await response.text(), /vrs-security-smoke/);
  },
);
