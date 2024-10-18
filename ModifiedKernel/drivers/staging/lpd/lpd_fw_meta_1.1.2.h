#ifndef _EXYNOS_LPD_METADATA_H_
#define _EXYNOS_LPD_METADATA_H_
/* DO NOT CHANGE as need to sync up with kernel driver !!!!!!!!!!!!!!!!! */

#define _VERSION(major, minor, revision) ((__u32)(((major) << 16) | (minor << 8) | (revision & 0xFF)))
#define LPD_METADATA_API_VERSION		_VERSION(1, 1, 2)

#define MAX_AUX_INFO				8
#define MAX_LAYOUT_AUX_INFO			5

#define MAX_RESOURCE_CNT			10
#define MAX_POS_CNT				10
#define MAX_LAYOUT_CNT				13

enum lpd_rsc_type {
	LPD_RSC_TYPE_NONE = 0,
	LPD_RSC_TYPE_BG,
	LPD_RSC_TYPE_CANVAS,
	LPD_RSC_TYPE_FG,
	LPD_RSC_TYPE_HANDS_HOUR,
	LPD_RSC_TYPE_HANDS_HOUR2,
	LPD_RSC_TYPE_HANDS_MIN,
	LPD_RSC_TYPE_HANDS_MIN2,
	LPD_RSC_TYPE_HANDS_SEC,
	LPD_RSC_TYPE_HANDS_PIVOT,
	LPD_RSC_TYPE_COLON1,
	LPD_RSC_TYPE_COLON2,
	LPD_RSC_TYPE_ICON1,
	LPD_RSC_TYPE_ICON2,
	LPD_RSC_TYPE_SYSTEM_ICON,
	LPD_RSC_TYPE_NUM_FONT1,
	LPD_RSC_TYPE_NUM_FONT2,
	LPD_RSC_TYPE_NUM_FONT3,
	LPD_RSC_TYPE_NUM_FONT4,
	LPD_RSC_TYPE_NUM_FONT5,
	LPD_RSC_TYPE_NUM_FONT6,
	LPD_RSC_TYPE_DIGITAL_AMPM1,
	LPD_RSC_TYPE_DIGITAL_AMPM2,
	LPD_RSC_TYPE_MAX,
} __attribute__((packed));

enum lpd_layout_type {
	LPD_LAYOUT_TYPE_NONE = 0,
	LPD_LAYOUT_TYPE_BG,
	LPD_LAYOUT_TYPE_CANVAS,
	LPD_LAYOUT_TYPE_FG,
	LPD_LAYOUT_TYPE_ANALOGCLOCK,		/* Analog clock */
	LPD_LAYOUT_TYPE_DIGITALCLOCK,		/* Digital clock */
	LPD_LAYOUT_TYPE_ICON1,			/* for watchface and app */
	LPD_LAYOUT_TYPE_ICON2,			/* for watchface and app */
	LPD_LAYOUT_TYPE_SYSTEM_ICON,		/* override system icon. not for watchface and app */
	LPD_LAYOUT_TYPE_NUM1,			/* update number count: ex) step, rate */
	LPD_LAYOUT_TYPE_NUM2,
	LPD_LAYOUT_TYPE_NUM3,
	LPD_LAYOUT_TYPE_NUM4,
	LPD_LAYOUT_TYPE_NUM5,
	LPD_LAYOUT_TYPE_NUM6,
	LPD_LAYOUT_TYPE_CLOCK,		/* drawing clock for Timer / duration / stopwatch app (optional) */
	LPD_LAYOUT_TYPE_DECIMAL_NUM1,
	LPD_LAYOUT_TYPE_DECIMAL_NUM2,
	LPD_LAYOUT_TYPE_DECIMAL_NUM3,
	LPD_LAYOUT_TYPE_DECIMAL_NUM4,
	LPD_LAYOUT_TYPE_MAX,
} __attribute__((packed));

enum lpd_img_fmt {
				/*      	|127       112|111        96|...|31         16|15          0| */
	ARGB8888	= 0x0,	/* 5'd0		|  A3  |  R3  |  G3  |  B3  |...|  A0  |  R0  |  G0  |	B0  | */
	ARGB1555	= 0xc,	/* 5'd12	|A| R6| G6| B6|A| R5| G5| B5|...|A| R1| G1| B1|A| R0| G0| B0| */
	ARGB4444	= 0xd,	/* 5'd13	|A7|R7 |G7 |B7|A6|R6 |G6 |B6|...|A1|R1 |G1 |B1|A0|R0 |G0 |B0| */
} __attribute__((packed));

enum img_buffer_type {
	LPD_RSC_BUF_TYPE_SRAM = 0,
	LPD_RSC_BUF_TYPE_DRAM,
	LPD_RSC_BUF_TYPE_MAX
} __attribute__((packed));

/* resource info */
struct lpd_resource_info {
	enum lpd_rsc_type rsc_type;
	unsigned int width;
	unsigned int height;
	enum lpd_img_fmt pixel_format;
	unsigned int aux_info[MAX_AUX_INFO];
	enum img_buffer_type buf_type;
	unsigned int buf_size;
	unsigned int addr;
};

enum lpd_rot_type {
	LPD_ROTATION_0 = 0,
	LPD_ROTATION_90,
	LPD_ROTATION_180,
	LPD_ROTATION_270,
};

struct lpd_pos_info {
	unsigned int x;
	unsigned int y;
};

/* layout info */
struct lpd_layout_info {
	enum lpd_layout_type layout_type;
	unsigned int z_order;
	enum lpd_rot_type rotation;
	unsigned int aux_info[MAX_AUX_INFO];
	struct lpd_pos_info pos_info[MAX_POS_CNT];
	enum lpd_rsc_type pos_rsc[MAX_POS_CNT];
};

enum lpd_rsc_buffer_id {
	LPD_RSC_ID_BG = 0,
	LPD_RSC_ID_CANV  = 1,
};

enum lpd_sensor_id {
	LPD_DATA_TYPE_OUT_EXERCISE_DURATION = 1,
	LPD_DATA_TYPE_OUT_EXERCISE_CAL,
	LPD_DATA_TYPE_OUT_EXERCISE_CUR_SPEED,
	LPD_DATA_TYPE_OUT_EXERCISE_AVG_SPEED,
	LPD_DATA_TYPE_OUT_EXERCISE_CUR_PACE = 5,
	LPD_DATA_TYPE_OUT_EXERCISE_AVG_PACE,
	LPD_DATA_TYPE_OUT_EXERCISE_DISTANCE,
	LPD_DATA_TYPE_OUT_EXERCISE_STEPS,
	LPD_DATA_TYPE_OUT_EXERCISE_CADENCE,
	LPD_DATA_TYPE_OUT_EXERCISE_ELEVATION = 10,
	LPD_DATA_TYPE_OUT_EXERCISE_LAP_DISTANCE = 11,
	LPD_DATA_TYPE_OUT_EXERCISE_LAP_DURATION = 12,
	LPD_DATA_TYPE_OUT_EXERCISE_LAP_AVERAGESPEED = 13,
	LPD_DATA_TYPE_OUT_EXERCISE_LAP_AVERAGEPACE = 14,
	LPD_DATA_TYPE_OUT_EXERCISE_LAP_CALORIE = 15,
	LPD_DATA_TYPE_OUT_EXERCISE_LAP_COUNT = 16,

	LPD_DATA_TYPE_IN_SWIM_DURATION = 20,
	LPD_DATA_TYPE_IN_SWIM_CAL = 21,
	LPD_DATA_TYPE_IN_SWIM_LENGTH = 22,
	LPD_DATA_TYPE_IN_SWIM_LAST_STROKE = 23,
	LPD_DATA_TYPE_IN_SWIM_DISTANCE = 24,
	LPD_DATA_TYPE_IN_SWIM_TOTAL_STROKE = 25,
	LPD_DATA_TYPE_IN_SWIM_LATEST_LENGTH = 26,
	LPD_DATA_TYPE_IN_SWIM_CURRENT_LEN_DURATION = 27,
	LPD_DATA_TYPE_IN_SWIM_LATEST_LEN_DURATION = 28,
	LPD_DATA_TYPE_IN_SWIM_LATEST_LEN_PACE = 29,
	LPD_DATA_TYPE_IN_SWIM_LATEST_LEN_TYPE = 30,

	LPD_DATA_TYPE_OUT_SWIM_DURATION = 31,
	LPD_DATA_TYPE_OUT_SWIM_DISTANCE,
	LPD_DATA_TYPE_OUT_SWIMM_CAL,
	LPD_DATA_TYPE_OUT_SWIM_TOTAL_STROKE = 34,
	LPD_DATA_TYPE_OUT_SWIM_AVG_PACE = 35,

	LPD_DATA_TYPE_ECP_DURATION = 41,
	LPD_DATA_TYPE_ECP_CAL = 42,

	LPD_DATA_TYPE_CUR_HEART = 51,
	LPD_DATA_TYPE_AVG_HEART,
	LPD_DATA_TYPE_MAX_HEART = 53,
	LPD_DATA_TYPE_PERCENT_VO2MAX = 54,
	LPD_DATA_TYPE_MAX
} __attribute__((packed));

#define MAX_SENSOR_DATA				7

enum wrist_evt_dest {
	WRIST_EVENT_AP = 0,
	WRIST_EVENT_CM55,
};

struct lpd_sensor_lux {
	unsigned int lux;
	unsigned int lux_level;
	unsigned int lux_updated;
} __attribute__((aligned(4), packed));

enum brightness_mode {
	AUTO_BR_MODE = 0,
	MANUAL_BR_MODE,
};
struct lpd_sensor_meta {
	unsigned int tick;
	unsigned int data_cnt;
	unsigned int wrist_evt_dest;
	unsigned int br_mode;
	struct lpd_sensor_lux lux_info;
	enum lpd_sensor_id id_list[MAX_SENSOR_DATA];
	int data[MAX_SENSOR_DATA];
} __attribute__((aligned(4), packed));


#define MAX_BRIGHTNESS_INFO	20

struct lpd_brightness_info {
	unsigned int physical_br ;	/* value for physical brightness(nit) */
	unsigned int platform_br;	/* value for brightness (platform) */
};

struct lpd_brightness_meta {
	unsigned int auto_br;	/* flag for using auto brightness, 0: manual, 1: auto */
	unsigned int br_cnt;
	struct lpd_brightness_info br_info[MAX_BRIGHTNESS_INFO];
};

/* meta info */
struct lpd_composer_meta {
	unsigned int lpd_metadata_version; 	/* meta_ver */
	unsigned int lpd_metadata_crc;		/* LSI added */
	unsigned int enable;
	unsigned int xres;
	unsigned int yres;
	unsigned int log_level;
	unsigned int resource_cnt;		/* LSI added */
	unsigned int layout_cnt;		/* LSI added */
	struct lpd_resource_info resources[MAX_RESOURCE_CNT];
	struct lpd_layout_info layouts[MAX_LAYOUT_CNT];
} __attribute__((aligned(4), packed));

struct lpd_dsim_meta {
	unsigned int hs_clk;

	unsigned int p;
	unsigned int m;
	unsigned int s;
	unsigned int k;

	unsigned int cmd_lp_ref;
};

struct lpd_panel_meta {
	unsigned int id;
	unsigned int xres;
	unsigned int yres;
	unsigned int fps;
};

struct lpd_dpu_meta {
	struct lpd_panel_meta panel_meta;	/* It should be same with panel size */
	struct lpd_dsim_meta dsim_meta;
} __attribute__((aligned(4), packed));


struct lpd_cmd_meta {
	unsigned int base;
	unsigned int size;
	unsigned int nit_tbl[MAX_BRIGHTNESS_INFO];
} __attribute__((aligned(4), packed));


struct lpd_comp_meta {
	unsigned int image_base;
	unsigned int image_size;

	unsigned int lut_base;
	unsigned int lut_size;

	unsigned int canvas_base;
	unsigned int canvas_size;
} __attribute__((aligned(4), packed));

struct lpd_time_meta {
	__s64 blend_time;
} __attribute__((aligned(4), packed));


#define LPD_DEBUG_META_CNT	10
struct lpd_debug_meta {
	unsigned int debug_info[LPD_DEBUG_META_CNT];
} __attribute__((aligned(4), packed));

#endif //_EXYNOS_LPD_METADATA_H
