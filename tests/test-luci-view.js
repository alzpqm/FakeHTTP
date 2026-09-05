'use strict';

const assert = require('assert');
const fsNode = require('fs');

const elements = {};
const buttons = [];
const maps = [];
let pollHandler;

function E(tag, attrs, children) {
	const element = {
		tag: tag,
		attrs: attrs || {},
		children: Array.isArray(children) ? children : [ children ],
		blur: function() {},
		className: '',
		disabled: false,
		textContent: '',
		getAttribute: function(name) { return this.attrs[name]; }
	};

	if (element.attrs.id)
		elements[element.attrs.id] = element;
	if (tag === 'button')
		buttons.push(element);

	return element;
}

function Option(name) {
	this.name = name;
	this.values = [];
}

Option.prototype.value = function(value, label) {
	this.values.push([ value, label ]);
};

function Section(type, name, title) {
	this.type = type;
	this.name = name;
	this.title = title;
	this.options = [];
	this.tabs = [];
}

Section.prototype.option = function(type, name, title) {
	const option = new Option(name);
	option.type = type;
	option.title = title;
	this.options.push(option);
	return option;
};

Section.prototype.tab = function(name, title) {
	this.tabs.push([ name, title ]);
};

Section.prototype.taboption = function(tab, type, name, title) {
	const option = this.option(type, name, title);
	option.tab = tab;
	return option;
};

function Map(name, title) {
	this.name = name;
	this.title = title;
	this.sections = [];
	this.readonly = false;
	maps.push(this);
}

Map.prototype.section = function(type, name, sectionName, title) {
	const section = new Section(type, name, title || sectionName);
	this.sections.push(section);
	return section;
};

Map.prototype.render = function() {
	return Promise.resolve({
		firstChild: {},
		inserted: null,
		insertBefore: function(node) { this.inserted = node; }
	});
};

const form = {
	Map: Map,
	TypedSection: function() {},
	GridSection: function() {},
	NamedSection: function() {},
	Flag: function() {},
	ListValue: function() {},
	Value: function() {},
	DynamicList: function() {}
};

const luciFs = {
	exec: function(path, args) {
		assert.strictEqual(path, '/etc/init.d/fakehttp');
		assert.ok([ 'status', 'start', 'restart', 'stop' ].includes(args[0]));
		return Promise.resolve({ code: 0, stdout: 'running\n', stderr: '' });
	}
};

const documentMock = {
	getElementById: function(id) { return elements[id] || null; },
	querySelectorAll: function(selector) {
		assert.strictEqual(selector, '#fakehttp_service_buttons button');
		return buttons;
	}
};

const source = fsNode.readFileSync(
	'openwrt/luci-app-fakehttp/htdocs/luci-static/resources/view/fakehttp.js',
	'utf8'
);
const factory = new Function(
	'view', 'form', 'fs', 'poll', 'ui', 'widgets', 'L', 'E', '_',
	'document', 'window', source
);
const view = factory(
	{ extend: function(value) { return value; } },
	form,
	luciFs,
	{ add: function(handler) { pollHandler = handler; } },
	{ addNotification: function() {} },
	{ DeviceSelect: function() {} },
	{ toArray: function(value) { return Array.isArray(value) ? value : [ value ]; } },
	E,
	function(value) { return value; },
	documentMock,
	{ setTimeout: function(resolve) { resolve(); } }
);

view.render().then(function(node) {
	assert.ok(node.inserted, 'service section was not inserted');
	assert.strictEqual(elements.fakehttp_status.attrs.role, 'status');
	assert.strictEqual(elements.fakehttp_status.attrs['aria-live'], 'polite');
	assert.strictEqual(elements.fakehttp_status.textContent, 'Running');
	assert.strictEqual(buttons.length, 3);
	assert.deepStrictEqual(buttons.map(function(button) {
		return button.attrs['data-action'];
	}), [ 'start', 'restart', 'stop' ]);
	assert.deepStrictEqual(buttons.map(function(button) {
		return button.disabled;
	}), [ true, false, false ]);

	assert.strictEqual(maps.length, 1);
	const advanced = maps[0].sections.find(function(section) {
		return section.name === 'advanced';
	});
	assert.ok(advanced, 'advanced section is missing');
	assert.deepStrictEqual(advanced.tabs.map(function(tab) { return tab[0]; }),
		[ 'packet', 'firewall' ]);

	const payloadSection = maps[0].sections.find(function(section) {
		return section.name === 'payload';
	});
	const payload = payloadSection.options.find(function(option) {
		return option.name === 'payload';
	});
	assert.strictEqual(payloadSection.options.find(function(option) {
		return option.name === 'comment';
	}).modalonly, true);

	function validatorContext(type, enabled) {
		return {
			map: {
				lookupOption: function(name) {
					return [ {
						formvalue: function() {
							return name === 'type' ? type : enabled;
						}
					} ];
				}
			}
		};
	}

	assert.strictEqual(payload.validate.call(
		validatorContext('http', '1'), 'row', 'www.example.com'), true);
	assert.notStrictEqual(payload.validate.call(
		validatorContext('https', '1'), 'row', "bad'host"), true);
	assert.strictEqual(payload.validate.call(
		validatorContext('binary', '1'), 'row', '/etc/fakehttp/payload'), true);
	assert.notStrictEqual(payload.validate.call(
		validatorContext('binary', '1'), 'row', 'relative/path'), true);
	assert.strictEqual(payload.validate.call(
		validatorContext('http', '0'), 'row', ''), true);
	assert.strictEqual(typeof pollHandler, 'function');

	return buttons[1].attrs.click({ preventDefault: function() {} });
}).then(function() {
	assert.strictEqual(elements.fakehttp_status.textContent, 'Running');
	console.log('LuCI view tests passed.');
}).catch(function(error) {
	console.error(error);
	process.exit(1);
});
