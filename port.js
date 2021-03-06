class Port {

	static async getPorts() {
		const devices = await navigator.usb.getDevices();
		return devices.map(device => new Port(device));
	};

	static async requestPort() {
		const filters = [
			{ 'vendorId': 0x2341, 'productId': 0x8036 },
			{ 'vendorId': 0x2341, 'productId': 0x8037 },
			{ 'vendorId': 0x2341, 'productId': 0x804d },
			{ 'vendorId': 0x2341, 'productId': 0x804e },
			{ 'vendorId': 0x2341, 'productId': 0x804f },
			{ 'vendorId': 0x2341, 'productId': 0x8050 },
		];

		const device = await navigator.usb.requestDevice({ filters });

		return new Port(device);
	}

	constructor(device) {
		this.device_ = device
	}

	async open() {
		await this.device_.open()

		if(this.device_.configuration === null) {
			await this.device_.selectConfiguration(1);
		}

		console.log(this.device_.configurations);

		for(const iface of this.device_.configuration.interfaces) {
			console.log('Interface %d:', iface.interfaceNumber, iface);
			for(const alt of iface.alternates) {
				const matches = alt.interfaceClass === 0xff;
				console.log('Alternate:', alt);
				if(matches) {
					this.interfaceNumber_ = iface.interfaceNumber;
				}
				for(const endpoint of alt.endpoints) {
					console.log('Endpoint %d:', endpoint.endpointNumber, endpoint);
					if(matches && endpoint.direction == 'out') {
						this.endpointOut_ = endpoint.endpointNumber;
					}
					if(matches && endpoint.direction == 'in') {
						this.endpointIn_ = endpoint.endpointNumber;
					}
				}
			}
		}

		await this.device_.claimInterface(this.interfaceNumber_);
		await this.device_.selectAlternateInterface(this.interfaceNumber_, 0);
		await this.device_.controlTransferOut({
			requestType: 'class',
			recipient: 'interface',
			request: 0x22,
			value: 0x01,
			index: this.interfaceNumber_
		});
	}

	read(cb) {
		let reading = true;
		function stop() {
			reading = false;
		}

		const loop = async () => {
			while(reading) {
				try {
					const result = await this.device_.transferIn(this.endpointIn_, 64);
					cb(null, result.data);
				} catch(e) {
					cb(e);
					break;
				}
			}
		};

		loop();

		return stop;
	}

	async close() {
		await this.device_.controlTransferOut({
			requestType: 'class',
			recipient: 'interface',
			request: 0x22,
			value: 0x00,
			index: this.interfaceNumber_
		});
		await this.device_.close();
	}

	async send(data) {
		const buffer = Uint8Array.from(data);
		return this.device_.transferOut(this.endpointOut_, buffer);
	}
}
