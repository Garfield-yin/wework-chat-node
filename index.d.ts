export interface InitParams {
	/** 企业ID */
	corpid: string;
	/** App Secret */
	secret: string;
	/**私钥，用于消息解密 */
	private_key: string;
	/** 数据拉取index */
	seq?: number;
}

export interface GetDataParams {
	/** 返回条数，最大1000 */
	max_results: number;
	/** 超时时间 单位 s */
	timeout: number;
	/** 数据拉取index */
	seq: number;
}

export interface GetMediaDataParams {
	/** 媒体资源的id信息 */
	sdk_fileid: string;
	/**媒体消息分片拉取，需要填入每次拉取的索引信息 */
	index_buf?: string;
}

export interface MediaDataResp {
	is_finished: boolean;
	buf_index: string;
	data: Buffer;
}

export interface ChatDataResp {
	msgid: string;
	action: string;
	from: string;
	tolist: string[];
	roomid: string;
	msgtime: number;
	// 暂支持不全
	msgtype:
		| "text"
		| "image"
		| "revoke"
		| "disagree"
		| "voice"
		| "video"
		| "location"
		| "emotion"
		| "file"
		| "link"
		| "weapp"
		| "chatrecord"
		| "todo"
		| "vote"
		| "redpacket"
		| "card";
	text?: {
		content: string;
	};
	image?: {
		md5sum: string;
		filesize: number;
		sdkfileid: string;
	};
	revoke?: {
		pre_msgid: string;
	};
	disagree?: {
		userid: string;
		disagree_time: number;
	};
	voice?: {
		md5sum: string;
		voice_size: number;
		sdkfileid: string;
		play_length: number;
	};
	video?: {
		md5sum: string;
		filesize: number;
		sdkfileid: string;
		play_length: number;
	};
	card?: {
		corpname: string;
		userid: string;
	};
	location?: {
		longitude: number;
		latitude: number;
		address: string;
		title: string;
		zoom: number;
	};
	emotion?: {
		type: 1 | 2;
		width: number;
		height: number;
		sdkfileid: string;
		md5sum: string;
		imagesize: number;
	};
	file?: {
		sdkfileid: string;
		md5sum: string;
		filename: string;
		fileext: string;
		filesize: number;
	};
	link?: {
		title: string; //	消息标题。String类型
		description: string; //	消息描述。String类型
		link_url: string; //	链接url地址。String类型
		image_url: string; //
	};
	weapp?: {
		title: string; //	消息标题。String类型
		description: string; //	消息描述。String类型
		username: string; //	用户名称。String类型
		displayname: string;
	};
	chatrecord?: any;
	todo?: {
		title: string;
		content: string;
	};
	vote?: any;
	redpacket?: any;
}

export interface CallbackFunc {
	(msg: string): void;
}
export interface WeWorkChat {
	getMediaData(params: GetMediaDataParams): MediaDataResp;
	fetchData(fn: CallbackFunc): any;
	stopFetch(): number;
	getChatData(params: GetDataParams): Array<string>;
}

export const WeWorkChat: {
	new (param: InitParams): WeWorkChat;
} = addon.WeWorkChat;
