'use strict';
'require dom';
'require view';
'require form';
'require fs';
'require poll';
'require ui';
'require tools.widgets as widgets';

function callInit(action) {
	return L.resolveDefault(fs.exec_direct('/etc/init.d/fakehttp', [ action ]), '');
}

function getStatus() {
	return callInit('status').then(function(res) {
		return String(res).trim() === 'running';
	});
}

function setStatus(running) {
	var status = document.getElementById('fakehttp_status');

	if (!status)
		return;

	status.textContent = running ? _('Running') : _('Stopped');
	status.className = running ? 'ifacebadge ifacebadge-active' : 'ifacebadge';
}

function setButtons(disabled) {
	document.querySelectorAll('#fakehttp_service_buttons button').forEach(function(btn) {
		btn.disabled = disabled;
		btn.blur();
	});
}

function handleAction(action) {
	var map = document.querySelector('.cbi-map');
	var run = Promise.resolve();

	setButtons(true);

	if (action === 'restart') {
		run = dom.callClassMethod(map, 'save')
			.then(L.bind(ui.changes.apply, ui.changes));
	}

	return run.then(function() {
		return callInit(action);
	}).then(function() {
		return getStatus();
	}).then(function(running) {
		setStatus(running);
		setButtons(false);
	}).catch(function(err) {
		setButtons(false);
		ui.addNotification(null, E('p', {}, [ err.message || err ]), 'error');
	});
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
		o.editable = true;
		o.rmempty = false;

		o = s.option(form.Value, 'payload', _('Payload'));
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
		o.datatype = 'or(-1,uinteger)';
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
				var service = E('div', { 'class': 'cbi-section' }, [
					E('h3', {}, _('Service')),
					E('p', {}, [
						_('Status'), ': ',
						E('span', { 'id': 'fakehttp_status' }, '')
					]),
					E('div', { 'id': 'fakehttp_service_buttons', 'class': 'right' }, [
						E('button', {
							'class': 'btn cbi-button cbi-button-apply',
							'click': function(ev) {
								ev.preventDefault();
								return handleAction('restart');
							}
						}, _('Restart')),
						' ',
						E('button', {
							'class': 'btn cbi-button cbi-button-reset',
							'click': function(ev) {
								ev.preventDefault();
								return handleAction('stop');
							}
						}, _('Stop'))
					])
				]);

				node.insertBefore(service, node.firstChild);
				setStatus(running);

				poll.add(function() {
					return getStatus().then(setStatus);
				});

				return node;
			});
		});
	}
});
