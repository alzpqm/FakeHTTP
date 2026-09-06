'use strict';
'require view';
'require form';
'require fs';
'require poll';
'require ui';
'require tools.widgets as widgets';

var serviceBusy = false;
var serviceReadonly = true;

var pageStyle = [
	'.fakehttp-service-row {',
	'  display: flex;',
	'  align-items: center;',
	'  justify-content: space-between;',
	'  gap: 1rem;',
	'  flex-wrap: wrap;',
	'}',
	'.fakehttp-status {',
	'  display: inline-flex;',
	'  align-items: center;',
	'  gap: .5rem;',
	'  min-width: 7.5rem;',
	'  min-height: 2rem;',
	'  font-weight: 600;',
	'  color: var(--text-color-high);',
	'}',
	'.fakehttp-status::before {',
	'  content: "";',
	'  width: .65rem;',
	'  height: .65rem;',
	'  flex: 0 0 .65rem;',
	'  border-radius: 50%;',
	'  background: var(--text-color-medium);',
	'  box-shadow: 0 0 0 1px var(--border-color-high);',
	'}',
	'.fakehttp-status.is-running::before {',
	'  background: var(--success-color-high);',
	'}',
	'.fakehttp-status.is-stopped::before {',
	'  background: var(--error-color-high);',
	'}',
	'.fakehttp-status.is-working::before {',
	'  background: var(--warn-color-high);',
	'  animation: fakehttp-pulse 1s ease-in-out infinite alternate;',
	'}',
	'.fakehttp-service-actions {',
	'  display: flex;',
	'  align-items: center;',
	'  justify-content: flex-end;',
	'  gap: .5rem;',
	'  flex-wrap: wrap;',
	'}',
	'.fakehttp-service-actions .cbi-button {',
	'  display: inline-flex;',
	'  align-items: center;',
	'  justify-content: center;',
	'  gap: .4rem;',
	'  min-width: 6.75rem;',
	'  min-height: 2.25rem;',
	'}',
	'.fakehttp-action-icon {',
	'  width: 1rem;',
	'  text-align: center;',
	'  font-size: 1rem;',
	'  line-height: 1;',
	'}',
	'@keyframes fakehttp-pulse { from { opacity: .45; } to { opacity: 1; } }',
	'@media (prefers-reduced-motion: reduce) {',
	'  .fakehttp-status.is-working::before { animation: none; }',
	'}',
	'@media (max-width: 600px) {',
	'  .fakehttp-service-row, .fakehttp-service-actions { width: 100%; }',
	'  .fakehttp-service-actions .cbi-button { flex: 1 1 7rem; }',
	'}'
].join('\n');

function callInit(action) {
	return fs.exec('/etc/init.d/fakehttp', [ action ]);
}

function getStatus() {
	return callInit('status').then(function(res) {
		return res.code === 0 && String(res.stdout).trim() === 'running';
	}).catch(function() {
		return null;
	});
}

function setStatus(running) {
	var status = document.getElementById('fakehttp_status');

	if (!status)
		return;

	status.textContent = running == null ? _('Unavailable') :
		(running ? _('Running') : _('Stopped'));
	status.className = 'fakehttp-status ' + (running == null ?
		'is-unavailable' : (running ? 'is-running' : 'is-stopped'));
}

function setWorkingStatus() {
	var status = document.getElementById('fakehttp_status');

	if (!status)
		return;

	status.textContent = _('Working...');
	status.className = 'fakehttp-status is-working';
}

function setButtons(running) {
	document.querySelectorAll('#fakehttp_service_buttons button').forEach(function(btn) {
		var action = btn.getAttribute('data-action');

		btn.disabled = serviceReadonly || serviceBusy || running == null ||
			(action === 'start' ? running : !running);
		if (btn.disabled)
			btn.blur();
	});
}

function updateService(running) {
	setStatus(running);
	setButtons(running);
}

function waitForStatus(expected, attempts) {
	return getStatus().then(function(running) {
		if (running === expected || attempts <= 1)
			return running;

		return new Promise(function(resolve) {
			window.setTimeout(resolve, 250);
		}).then(function() {
			return waitForStatus(expected, attempts - 1);
		});
	});
}

function handleAction(action) {
	var expected = action !== 'stop';

	serviceBusy = true;
	setWorkingStatus();
	setButtons(null);

	return callInit(action).then(function(res) {
		if (res.code !== 0) {
			throw new Error(String(res.stderr || res.stdout ||
				_('Service action failed')).trim());
		}
		return waitForStatus(expected, 20);
	}).then(function(running) {
		if (running == null)
			throw new Error(_('Unable to read FakeHTTP service status'));
		if (expected && !running)
			throw new Error(_('FakeHTTP did not start'));
		if (!expected && running)
			throw new Error(_('FakeHTTP did not stop'));

		serviceBusy = false;
		updateService(running);
	}).catch(function(err) {
		serviceBusy = false;
		ui.addNotification(null, E('p', {}, [ err.message || err ]), 'error');
		return getStatus().then(updateService);
	});
}

function validatePayload(section_id, value) {
	var typeOption = L.toArray(this.map.lookupOption('type', section_id))[0];
	var enabledOption = L.toArray(this.map.lookupOption('enabled', section_id))[0];
	var type = typeOption ? typeOption.formvalue(section_id) : 'http';
	var enabled = enabledOption ? enabledOption.formvalue(section_id) : '1';

	if (enabled === '0')
		return true;

	if (!value)
		return _('A payload is required');
	if (/[\x00-\x1f\x7f]/.test(value))
		return _('Control characters are not allowed');

	if (type === 'binary')
		return value.length > 4095 ? _('Binary payload path is too long') :
			(value.charAt(0) === '/' ? true : _('Binary payload path must be absolute'));

	if (value.length > 253)
		return _('Host name is too long');
	if (!/^(?:[A-Za-z0-9](?:[A-Za-z0-9-]{0,61}[A-Za-z0-9])?\.)*[A-Za-z0-9](?:[A-Za-z0-9-]{0,61}[A-Za-z0-9])?$/.test(value))
		return _('Enter a valid host name');

	return true;
}

return view.extend({
	render: function() {
		var m, s, o;

		m = new form.Map('fakehttp', _('FakeHTTP'));

		s = m.section(form.TypedSection, 'globals', _('General'));
		s.anonymous = true;
		s.addremove = false;

		o = s.option(form.Flag, 'enabled', _('Enable'));
		o.rmempty = false;

		o = s.option(form.Flag, 'silent', _('Silent mode'));
		o.default = '1';
		o.rmempty = false;

		o = s.option(widgets.DeviceSelect, 'interface', _('Interfaces'));
		o.multiple = true;
		o.nocreate = true;
		o.rmempty = false;

		s = m.section(form.GridSection, 'payload', _('Payloads'));
		s.anonymous = true;
		s.addremove = true;
		s.addbtntitle = _('Add payload');
		s.sortable = true;
		s.nodescriptions = true;

		o = s.option(form.Flag, 'enabled', _('Enable'));
		o.default = '1';
		o.editable = true;
		o.rmempty = false;

		o = s.option(form.ListValue, 'type', _('Type'));
		o.value('http', 'HTTP');
		o.value('https', 'HTTPS');
		o.value('binary', _('Binary'));
		o.default = 'http';
		o.editable = true;
		o.rmempty = false;

		o = s.option(form.Value, 'payload', _('Payload'));
		o.validate = validatePayload;
		o.editable = true;
		o.placeholder = 'www.example.com';
		o.rmempty = false;

		o = s.option(form.Value, 'comment', _('Comment'));
		o.modalonly = true;
		o.rmempty = true;

		s = m.section(form.NamedSection, 'advanced', 'advanced', _('Advanced'));
		s.anonymous = true;
		s.addremove = false;
		s.tab('packet', _('Packet'));
		s.tab('firewall', _('Firewall'));

		o = s.taboption('firewall', form.Flag, 'skip', _('Skip firewall rules'));
		o.rmempty = false;

		o = s.taboption('packet', form.Flag, 'disable_estimation',
			_('Disable hop estimation'));
		o.rmempty = false;

		o = s.taboption('packet', form.Value, 'pct', _('Dynamic TTL percentage'));
		o.datatype = 'or(-1,range(1,99))';
		o.placeholder = '-1';
		o.rmempty = false;

		o = s.taboption('firewall', form.Value, 'fwmark_bypassing',
			_('Firewall mark'));
		o.datatype = 'or(-1,uinteger)';
		o.placeholder = '-1';
		o.rmempty = false;

		o = s.taboption('firewall', form.Value, 'fwmark_handle',
			_('Firewall mark mask'));
		o.datatype = 'or(-1,uinteger)';
		o.placeholder = '-1';
		o.rmempty = false;

		o = s.taboption('firewall', form.Value, 'queue_number',
			_('NFQUEUE number'));
		o.datatype = 'or(-1,range(0,65535))';
		o.placeholder = '-1';
		o.rmempty = false;

		o = s.taboption('firewall', form.DynamicList, 'bypass_port',
			_('Bypass TCP ports'));
		o.datatype = 'range(1,65535)';
		o.placeholder = '65499';
		o.rmempty = true;

		o = s.taboption('packet', form.Value, 'repeat', _('Packet repeat'));
		o.datatype = 'or(-1,range(1,10))';
		o.placeholder = '-1';
		o.rmempty = false;

		o = s.taboption('packet', form.Value, 'ttl', _('TTL'));
		o.datatype = 'or(-1,range(1,255))';
		o.placeholder = '-1';
		o.rmempty = false;

		o = s.taboption('firewall', form.Flag, 'use_iptables', _('Use iptables'));
		o.rmempty = false;

		return getStatus().then(function(running) {
			return m.render().then(function(node) {
				serviceReadonly = m.readonly === true;
				var service = E('div', { 'class': 'cbi-section' }, [
					E('style', { 'type': 'text/css' }, pageStyle),
					E('h3', {}, _('Service')),
					E('div', { 'class': 'cbi-section-node' }, [
						E('div', { 'class': 'fakehttp-service-row' }, [
							E('span', {
								'id': 'fakehttp_status',
								'class': 'fakehttp-status',
								'role': 'status',
								'aria-live': 'polite',
								'aria-atomic': 'true'
							}, ''),
							E('div', {
								'id': 'fakehttp_service_buttons',
								'class': 'fakehttp-service-actions'
							}, [
								E('button', {
								'type': 'button',
								'data-action': 'start',
								'class': 'btn cbi-button cbi-button-positive',
								'title': _('Start FakeHTTP'),
								'click': function(ev) {
									ev.preventDefault();
									return handleAction('start');
								}
								}, [
								E('span', {
									'class': 'fakehttp-action-icon',
									'aria-hidden': 'true'
								}, '\u25b6'),
								_('Start')
								]),
								E('button', {
								'type': 'button',
								'data-action': 'restart',
								'class': 'btn cbi-button cbi-button-apply',
								'title': _('Restart FakeHTTP'),
								'click': function(ev) {
									ev.preventDefault();
									return handleAction('restart');
								}
								}, [
								E('span', {
									'class': 'fakehttp-action-icon',
									'aria-hidden': 'true'
								}, '\u21bb'),
								_('Restart')
								]),
								E('button', {
								'type': 'button',
								'data-action': 'stop',
								'class': 'btn cbi-button cbi-button-negative',
								'title': _('Stop FakeHTTP'),
								'click': function(ev) {
									ev.preventDefault();
									return handleAction('stop');
								}
								}, [
								E('span', {
									'class': 'fakehttp-action-icon',
									'aria-hidden': 'true'
								}, '\u25a0'),
								_('Stop')
								])
							])
						])
					])
				]);

				node.insertBefore(service, node.firstChild);
				updateService(running);

				poll.add(function() {
					if (serviceBusy)
						return Promise.resolve();

					return getStatus().then(updateService);
				});

				return node;
			});
		});
	}
});
