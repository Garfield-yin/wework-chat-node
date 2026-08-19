/** 各示例共用的凭据读取逻辑。凭据一律来自环境变量。 */
module.exports = function loadConfig() {
	const { WEWORK_CORPID, WEWORK_SECRET, WEWORK_PRIVATE_KEY } = process.env;

	if (!WEWORK_CORPID || !WEWORK_SECRET || !WEWORK_PRIVATE_KEY) {
		console.error(
			"请先设置环境变量：\n" +
				"  export WEWORK_CORPID=ww00000000000000\n" +
				"  export WEWORK_SECRET=your-chat-archive-secret\n" +
				'  export WEWORK_PRIVATE_KEY="$(cat /path/to/private_key.pem)"'
		);
		process.exit(1);
	}

	return {
		corpid: WEWORK_CORPID,
		secret: WEWORK_SECRET,
		private_key: WEWORK_PRIVATE_KEY,
		seq: 0,
	};
};
