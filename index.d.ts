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
	/** 返回条数，最大 1000。不传或超出 1~1000 时按 1000 处理 */
	max_results?: number;
	/** 超时时间 单位 s，不传默认 30 */
	timeout?: number;
	/** 数据拉取 index，从该 seq 之后开始拉取。不传默认 0 */
	seq?: number;
}

export interface GetMediaDataParams {
	/** 媒体资源的id信息 */
	sdk_fileid: string;
	/** 媒体消息分片拉取，需要填入每次拉取的索引信息。首次拉取可不传 */
	index_buf?: string;
}

export interface MediaDataResp {
	is_finished: boolean;
	buf_index: string;
	data: Buffer;
}

export interface ChatDataItem {
	msgid: string;
	action: string;
	from: string;
	tolist: string[];
	roomid: string;
	msgtime: number;
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
		| "collect"
		| "redpacket"
		| "card"
		| "meeting"
		| "docmsg"
		| "markdown"
		| "news"
		| "calendar"
		| "mixed"
		| "meeting_voice_call"
		| "voip_doc_share"
		| "external_redpacket"
		| "sphfeed";

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
	collect?: {
		room_name: string;
		creator: string;
		create_time: string;
		title: string;
		details: {
			id: number;
			ques: string;
			type: "Text" | "Number" | "Date" | "Time" | "String";
		}[];
	};
	redpacket?: {
		type: number;
		wish: string;
		totalcnt: number;
		totalamount: number;
	};
	meeting?: {
		topic: string;
		starttime: number;
		endtime: number;
		address: string;
		remarks: string;
		meetingtype: number;
		meetingid: number;
		status: number;
	};
	docmsg?: any;
	markdown?: any;
	news?: any;
	calendar?: any;
	mixed?: any;
	meeting_voice_call?: any;
	voip_doc_share?: any;
	external_redpacket?: any;
	sphfeed?: any;
}

export interface ChatDataResp {
	/**
	 * 本批最后一条数据的 seq。
	 * 本批没有数据时为 0，轮询时请自行保留上一次的 seq，不要直接赋值回去。
	 */
	last_seq: number;
	/**
	 * 解密后的消息 JSON 字符串数组，每一项可用 JSON.parse 得到 ChatDataItem。
	 * 私钥版本不匹配等原因导致解密失败的消息会留空，遍历时请跳过空值。
	 */
	data: (string | undefined)[];
}

export interface CallbackFunc {
	(msg: string): void;
}
export interface WeWorkChat {
	getMediaData(params: GetMediaDataParams): MediaDataResp;
	/**
	 * 回调形式：成功时回调 (null, resp) 并返回 null；失败时回调 (errMsg) 并返回 -1。
	 */
	getMediaData(
		params: GetMediaDataParams,
		cb: (err: string | null, resp?: MediaDataResp) => void
	): null | number;
	/**
	 * 启动后台线程持续拉取消息，每条消息回调一次。
	 * 同一实例同时只能有一个 fetchData 在跑，返回的 Promise 在 stopFetch 之后 resolve。
	 */
	fetchData(fn: CallbackFunc): Promise<boolean>;
	/**
	 * 停止 fetchData 并释放 sdk，返回停止时已拉取到的最后一个 seq。
	 * 会阻塞等待后台线程真正退出（最长 45s），以保证返回的 seq 是最终值、
	 * 且 sdk 不会在使用中被释放。调用之后该实例不能再发起请求。
	 */
	stopFetch(): number;
	getChatData(params: GetDataParams): ChatDataResp;
}

export const WeWorkChat: {
	new (param: InitParams): WeWorkChat;
};
