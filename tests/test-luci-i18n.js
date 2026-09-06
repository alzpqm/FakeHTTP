'use strict';

const assert = require('assert');
const fs = require('fs');

const view = fs.readFileSync(
	'openwrt/luci-app-fakehttp/htdocs/luci-static/resources/view/fakehttp.js',
	'utf8'
);
const po = fs.readFileSync(
	'openwrt/luci-app-fakehttp/po/zh_Hant/fakehttp.po',
	'utf8'
);

const messages = new Map();
const entryPattern = /^msgid ("(?:\\.|[^"\\])*")\nmsgstr ("(?:\\.|[^"\\])*")$/gm;
let match;

while ((match = entryPattern.exec(po)) !== null) {
	const source = JSON.parse(match[1]);
	const translation = JSON.parse(match[2]);

	if (source)
		messages.set(source, translation);
}

const required = new Set();
const messagePattern = /_\('([^']+)'\)/g;

while ((match = messagePattern.exec(view)) !== null)
	required.add(match[1]);

for (const source of required) {
	assert.ok(messages.has(source), `missing zh-Hant translation: ${source}`);
	assert.ok(messages.get(source).trim(), `empty zh-Hant translation: ${source}`);
}

assert.strictEqual(messages.size, required.size,
	'translation catalog contains stale or unreferenced messages');
console.log(`LuCI zh-Hant translations passed (${required.size} messages).`);
