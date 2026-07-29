'use strict';
'require view';
'require form';
'require fs';
'require poll';
'require ui';
'require tools.widgets as widgets';

var serviceBusy = false;
var serviceReadonly = true;

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
	status.className = running ? 'ifacebadge ifacebadge-active' : 'ifacebadge';
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
	setButtons(null);

	return callInit(action).then(function(res) {
		if (res.code !== 0) {
			throw new Error(String(res.stderr || res.stdout ||
				_('Service action failed')).trim());
		}
		return waitForStatus(expected, 9);
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
		return value.charAt(0) === '/' ? true : _('Binary payload path must be absolute');

	if (value.length > 253)
		return _('Host name is too long');
	if (/[^\x21-\x7e]/.test(value) || /[\/\\]/.test(value))
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
		o.rmempty = false;

		o = s.option(form.Value, 'comment', _('Comment'));
		o.editable = true;
		o.rmempty = true;

		s = m.section(form.NamedSection, 'advanced', 'advanced', _('Advanced'));
		s.anonymous = true;
		s.addremove = false;

		o = s.option(form.Flag, 'skip', _('Skip firewall rules'));
		o.rmempty = false;

		o = s.option(form.Flag, 'disable_estimation', _('Disable hop estimation'));
		o.rmempty = false;

		o = s.option(form.Value, 'pct', _('Dynamic TTL percentage'));
		o.datatype = 'or(-1,range(1,99))';
		o.placeholder = '-1';
		o.rmempty = false;

		o = s.option(form.Value, 'fwmark_bypassing', _('Firewall mark'));
		o.datatype = 'or(-1,uinteger)';
		o.placeholder = '-1';
		o.rmempty = false;

		o = s.option(form.Value, 'fwmark_handle', _('Firewall mark mask'));
		o.datatype = 'or(-1,uinteger)';
		o.placeholder = '-1';
		o.rmempty = false;

		o = s.option(form.Value, 'queue_number', _('NFQUEUE number'));
		o.datatype = 'or(-1,range(0,65535))';
		o.placeholder = '-1';
		o.rmempty = false;

		o = s.option(form.Value, 'repeat', _('Packet repeat'));
		o.datatype = 'or(-1,range(1,10))';
		o.placeholder = '-1';
		o.rmempty = false;

		o = s.option(form.Value, 'ttl', _('TTL'));
		o.datatype = 'or(-1,range(1,255))';
		o.placeholder = '-1';
		o.rmempty = false;

		o = s.option(form.Flag, 'use_iptables', _('Use iptables'));
		o.rmempty = false;

		return getStatus().then(function(running) {
			return m.render().then(function(node) {
				serviceReadonly = m.readonly === true;
				var service = E('div', { 'class': 'cbi-section' }, [
					E('h3', {}, _('Service')),
					E('div', { 'class': 'cbi-section-node' }, [
						E('div', { 'class': 'cbi-value' }, [
							E('label', { 'class': 'cbi-value-title' }, _('Status')),
							E('div', { 'class': 'cbi-value-field' }, [
								E('span', { 'id': 'fakehttp_status' }, '')
							])
						]),
						E('div', {
							'id': 'fakehttp_service_buttons',
							'class': 'cbi-page-actions'
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
							}, _('Start')),
							' ',
							E('button', {
								'type': 'button',
								'data-action': 'restart',
								'class': 'btn cbi-button cbi-button-apply',
								'title': _('Restart FakeHTTP'),
								'click': function(ev) {
									ev.preventDefault();
									return handleAction('restart');
								}
							}, _('Restart')),
							' ',
							E('button', {
								'type': 'button',
								'data-action': 'stop',
								'class': 'btn cbi-button cbi-button-negative',
								'title': _('Stop FakeHTTP'),
								'click': function(ev) {
									ev.preventDefault();
									return handleAction('stop');
								}
							}, _('Stop'))
						])
					])
				]);

				node.insertBefore(service, node.firstChild);
				updateService(running);

				poll.add(function() {
					return getStatus().then(updateService);
				});

				return node;
			});
		});
	}
});
