#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


GList *public_name_alias = NULL;
gint mentioned_array [54][54];
gint mentioned_count [54];
gint been_mentioned_count [54];
float mentioned_rate_array [54][54];
float been_mentioned_rate_array [54][54];

gint timeline_array[54][54];
/*bf: been follow; f: follow */
gint timeline_bf_count [54];
gint timeline_f_count [54];
float timeline_bf_rate_array[54][54];
float timeline_f_rate_array[54][54];

enum msg_type {
	MSG_WORD,
	MSG_EXPRESS,
	MSG_PIC,
	MSG_VOICE,
	MSG_LINK,
	MSG_SYSTEM,
	MSG_NULL
};

enum head_type {
	HEAD_DATE,
	HEAD_TIME,
	HEAD_NAME,
	HEAD_SR,
	HEAD_MSG_TYPE,
	HEAD_NULL
};

typedef struct _wechat_msg {
	gchar *head[5]; 
	gchar *content;
} wechat_msg;

typedef struct _name_alias {
	gchar *name;
	gint id;
} name_alias;

enum load_status {
	LOAD_INIT,
	LOAD_START,
	LOAD_END,
	LOAD_NULL
};

enum line_type {
	LINE_BOUND,
	LINE_HEAD,
	LINE_CONTENT,
	LINE_OTHER
};

static void
msg_debug (wechat_msg *msg)
{
	gint i;
	printf ("Msg head is: \t\t");
	for (i = 0; (msg->head[i]) && (i < 5); i++) {
		printf ("[%s]\t", msg->head[i]);
	}
	printf ("\t\t Content is: [%s]\n", msg->content);
}

static gint
check_line_type (gchar *line)
{
	gint type;
	gchar *line_head = "－－－－－－－－－－－－－－－－－－－－－－－";
	/*FIXME: date_head should have better policy */
	gchar *date_head_13 = "2013-";
	gchar *date_head_14 = "2014-";
	gchar *system_info = "系统通知";
	if (strncmp (line, date_head_13, strlen(date_head_13)) == 0) {
		return LINE_HEAD;
	} else if (strncmp (line, date_head_14, strlen(date_head_14)) == 0) {
		return LINE_HEAD;
	} else if (strncmp (line, line_head, strlen (line_head)) == 0) {
		return LINE_BOUND;
	} else if (strstr (line, system_info)) {	/*It does not work for utf8 */
		return LINE_OTHER;
	} 
	return LINE_CONTENT;
}
				
static wechat_msg *
load_msg_head (gchar *line)
{
	wechat_msg *msg = NULL;
        gchar **argv;
	gint i;

        if (!line)
                return NULL;
        argv = g_strsplit_set (line, "\n ", -1);
	msg = (wechat_msg *) malloc (sizeof (wechat_msg));
	for (i = 0; i < 5; i++) {
		msg->head[i] = NULL;
	}
	msg->content = NULL;
	gchar *p;
	gchar *s_arg;
	i = 0;
	for (p = line, s_arg = line; *p; p++) {
		if ((*p == ' ') || (*p == '\t') || (*p == '\n')) {
			if (s_arg) {
				*p = '\0';
				if (i < 5)
					msg->head[i] = g_strdup (s_arg);
				else 
					msg->content = g_strdup (s_arg);
				i++;
				s_arg = NULL;
			}
		} else if (s_arg == NULL) {
			s_arg = p;
		}
		
	}

	gint found = 0;
	for (i = 0; i < 5; i++) {
		if (msg->head[i] == NULL) {
		found = 1;
		break;
		}
	}
	if (found == 1) {
		/* a part of content in fact, TODO, should fix it */
//		printf ("this message is not valid\n");
		g_free (msg);
		return NULL;
	}
	return msg;
}

static void
append_content (wechat_msg *msg, gchar *content)
{
	if (!msg->content) {
		msg->content = g_strdup (content);
	} else {
		gchar *tmp;
		tmp = g_strdup_printf ("%s%s", msg->content, content);
		g_free (msg->content);
		msg->content = tmp;
	}
}

static GList *
load_content (gchar *filename)
{
        gchar *content = NULL;
        gsize length;
        gchar **argv;
	gint i;
	wechat_msg *msg = NULL;
	gint status = LOAD_INIT;
	gint line_type = LINE_OTHER;
	GList *msgs = NULL;

        g_file_get_contents (filename, &content, &length, NULL);
        if (!content)
                return;

        argv = g_strsplit (content, "\n", -1);
        for (i = 0; argv[i]; i++) {
		gchar *line = argv[i];
		line_type = check_line_type (line);
		if (line_type == LINE_BOUND) {
			if (status == LOAD_INIT) {
				status = LOAD_START;
			} else {
				status = LOAD_END;
				if (msg) {
/*We use prepend,  because the log is upside down.. */
					msgs = g_list_prepend (msgs, msg);
				}
				break;
			}
		} else if (line_type == LINE_HEAD) {
			if (msg) {
				msgs = g_list_prepend (msgs, msg);
			}
			msg = load_msg_head (line);
		} else if (line_type == LINE_CONTENT) {
			if (msg) {
				append_content (msg, line);
			}
		} else if (line_type == LINE_OTHER) {
			/* no use */
		}
        }

        g_strfreev (argv);
        g_free (content);

	return msgs;
}

static void
user_been_at_count (GList *msgs)
{
	GList *l;
	wechat_msg *msg;

	gint tails = 0;
	gchar *names[128];
	gint counts[128];
	gint i;
	gint found;

	for (i = 0; i < 128; i++) {
		names[i] = NULL;
		counts[i] = 0;
	}

	for (l = msgs; l; l = l->next) {
		gchar *content;
		gchar *name;
		gchar *p_at, *p;
		msg = (wechat_msg *) (l->data);
		content = msg->content;
		if (!content)
			continue;

		p_at = strchr (content, '@');
		if (p_at) {
			found = 0;
			name = p_at + 1;
			for (p = name; *p; p++) {
				if ((*p == ' ') || (*p == '?'))
					*p = '\0';
			}
			for (i = 0; i < tails; i++) {
				if (strcmp (names[i], name) == 0) {
					found = 1;
					break;
				}
			}
			if (!found) {
				names[tails] = g_strdup (name);
				i = tails;
				tails ++;
			}
			counts [i] ++;
		}
	}

	for (i = 0; i < tails; i++) {
		printf ("%05d [%s]\n", counts[i], names[i]);
	}
}

static void
user_count (GList *msgs)
{
	GList *l;
	wechat_msg *msg;

	gint tails = 0;
	gchar *names[128];
	gint counts[128];
	gint i;
	gint found;

	for (i = 0; i < 128; i++) {
		names[i] = NULL;
		counts[i] = 0;
	}

	for (l = msgs; l; l = l->next) {
		gchar *name;
		msg = (wechat_msg *) (l->data);
		name = msg->head[HEAD_NAME];
		found = 0;
		for (i = 0; i < tails; i++) {
			if (strcmp (names[i], name) == 0) {
				found = 1;
				break;
			}
		}
		if (!found) {
			names[tails] = g_strdup (name);
			i = tails;
			tails ++;
		}
		counts [i] ++;
	}

	for (i = 0; i < tails; i++) {
		printf ("%05d [%s]\n", counts[i], names[i]);
	}
}

static void
day_count (GList *msgs)
{
	gchar *days[512];
	gint   days_counts[512];
	gint i;
	GList *l;
	gint tail;
	wechat_msg *msg;
	
	for (i = 0; i < 512; i ++) {
		days[i] = NULL;
		days_counts[i] = 0;	
	}
	tail = -1;
	for (l = msgs; l ; l = l->next) {
		gchar *day;
		msg = (wechat_msg *) (l->data);
		day = msg->head[HEAD_DATE];

		if ((tail == -1) || (strcmp (days[tail], day) != 0)) {
			tail ++;
			days[tail] = g_strdup (day);
		} else {
			/* already exist */
		}
			
		days_counts [tail] ++;
	}

	printf ("Day count\n");
	for (i = 0; i < tail; i++) {
		printf ("%05d [%s]\n", days_counts[i], days[i]);
	}

	printf ("\n\nWeek count\n");
	int j;
	for (i = 0; i < tail; i = i + 7) {
		gint week_count = 0;
	 	for (j = 0; j < 7; j++) {
			week_count += days_counts[i+j];
		}
		
		printf ("%05d [%s]\n", week_count, days[i]);
	}
}

		
static gint
if_new_topic (wechat_msg *msg, wechat_msg *last_msg)
{
	if (msg == NULL)
		return 0;
	if (last_msg == NULL)
		return 1;
	/* if no body reply in 15 minutes */
	gchar *new_time = msg->head[HEAD_TIME];
	gchar *last_time = last_msg->head[HEAD_TIME];
	gint new_i;
	gint last_i;
	gint gap;
        char *p;
	if (new_time == NULL)
		return 0;
	if (last_time == NULL)
		return 1;

        p = strchr (new_time, ':');
	if (!p)
		return 0;
        new_i = atoi (new_time)*60+ atoi(p+1);

        p = strchr (last_time, ':');
	if (!p)
		return 1;
        last_i = atoi (last_time)*60+ atoi(p+1);

	if (new_i < last_i)
		new_i + 60*24;
	gap = new_i - last_i;
	
	if (gap > 15) {
		return 1;
	} else {
		return 0;
	}
}

static int
is_shafa (wechat_msg *msg, wechat_msg *next_msg)
{
	if ((msg == NULL) || (next_msg == NULL))
		return 0;

	gchar *new_time = msg->head[HEAD_TIME];
	gchar *last_time = next_msg->head[HEAD_TIME];
	gint new_i;
	gint last_i;
	gint gap;
        char *p;
	if (new_time == NULL)
		return 0;
	if (last_time == NULL)
		return 0;

        p = strchr (new_time, ':');
	if (!p)
		return 0;
        new_i = atoi (new_time)*60+ atoi(p+1);

        p = strchr (last_time, ':');
	if (!p)
		return 1;
        last_i = atoi (last_time)*60+ atoi(p+1);

	if (new_i < last_i)
		new_i + 60*24;
	gap = new_i - last_i;
	
/* 5分钟内响应算沙发 */
	if (gap < 5) {
		return 1;
	} else {
		return 0;
	}
}

/* who start most topic with reply */
static void
topic_start (GList *msgs)
{
	GList *l;
	wechat_msg *msg;

	gint new_tails = 0;
	gint shafa_tails = 0;
	gint end_tails = 0;
	gchar *new_names[128];
	gint new_counts[128];
	gchar *shafa_names[128];
	gint shafa_counts[128];
	gchar *end_names[128];
	gint end_counts[128];
	gint i;
	gint found;
	wechat_msg *last_msg = NULL;

	for (i = 0; i < 128; i++) {
		new_names[i] = NULL;
		new_counts[i] = 0;
		shafa_names[i] = NULL;
		shafa_counts[i] = 0;
		end_names[i] = NULL;
		end_counts[i] = 0;
	}

	for (l = msgs; l; l = l->next) {
		gchar *head_time;
		gchar *name;
		gchar *p_at, *p;
		gint new_topic;

		msg = (wechat_msg *) (l->data);
		head_time = msg->head[HEAD_TIME];

		if (!head_time)
			continue;

		new_topic = if_new_topic (msg, last_msg);
		/*话题开始者*/
		if (new_topic) {
			gint new_i;
			found = 0;
			gchar *new_name = msg->head[HEAD_NAME];
			for (new_i = 0; new_i < new_tails; new_i++) {
				if (strcmp (new_names[new_i], new_name) == 0) {
					found = 1;
					break;
				}
			}
			if (!found) {
				new_names[new_tails] = g_strdup (new_name);
				new_i = new_tails;
				new_tails ++;
			}
			new_counts [new_i] ++;
		}

		/*沙发*/
		GList *next_l;
		wechat_msg *next_msg = NULL;
		next_l = l->next;
		if (new_topic && next_l) {
			next_msg = (wechat_msg *) next_l->data;
			if (is_shafa (msg, next_msg)) {
				gint shafa_i;
				gchar *shafa_name = next_msg->head[HEAD_NAME];
				found = 0;
				for (shafa_i = 0; shafa_i < shafa_tails; shafa_i++) {
					if (strcmp (shafa_names[shafa_i], shafa_name) == 0) {
						found = 1;
						break;
					}
				}
				if (!found) {
					shafa_names[shafa_i] = g_strdup (shafa_name);
					shafa_i = shafa_tails;
					shafa_tails ++;
				}
				shafa_counts[shafa_i]++;
			}
		}

		/*话题终结者 */
		if (new_topic && (last_msg)) {
			gint end_i;
			found = 0;
			gchar *end_name = last_msg->head[HEAD_NAME];
			for (end_i = 0; end_i < end_tails; end_i++) {
				if (strcmp (end_names[end_i], end_name) == 0) {
					found = 1;
					break;
				}
			}
			if (!found) {
				end_names[end_tails] = g_strdup (end_name);
				end_i = end_tails;
				end_tails ++;
			}
			end_counts [end_i] ++;
		}
		last_msg = msg;
	}

	printf ("\n\nStart topic\n");
	for (i = 0; i < new_tails; i++) {
		printf ("%05d [%s]\n", new_counts[i], new_names[i]);
	}

	printf ("\n\nShafa topic\n");
	for (i = 0; i < shafa_tails; i++) {
		printf ("%05d [%s]\n", shafa_counts[i], shafa_names[i]);
	}
	printf ("\n\nEnd topic\n");
	for (i = 0; i < end_tails; i++) {
		printf ("%05d [%s]\n", end_counts[i], end_names[i]);
	}
}

static void
express_count (GList *msgs)
{
	GList *l;
	wechat_msg *msg;

	gint tails = 0;
	gchar *names[128];
	gint counts[128];
	gint i;
	gint found;

	for (i = 0; i < 128; i++) {
		names[i] = NULL;
		counts[i] = 0;
	}

	for (l = msgs; l; l = l->next) {
		gchar *content;
		gchar *name;
		gchar *p;

		found = 0;
		msg = (wechat_msg *) (l->data);
		if (strcmp (msg->head[HEAD_MSG_TYPE], "表情") == 0) {
			found == 1;
		} else {
			content = msg->content;
			if (!content)
				continue;
			p = strchr (content, '[');
			if (p) {
				found = 1;
			}
		}
		if (!found)
			continue;
		found = 0;
		name = msg->head[HEAD_NAME];
		for (i = 0; i < tails; i++) {
			if (strcmp (names[i], name) == 0) {
				found = 1;
				break;
			}
		}
			
		if (!found) {
			names[tails] = g_strdup (name);
			i = tails;
			tails ++;
		}
		counts [i] ++;
	}

	for (i = 0; i < tails; i++) {
		printf ("%05d [%s]\n", counts[i], names[i]);
	}
}

static void
express_rate_count (GList *msgs)
{
	GList *l;
	wechat_msg *msg;

	gint tails = 0;
	gchar *names[128];
	gint whole_counts[128];
	gint counts[128];
	gint i;
	gint found;

	for (i = 0; i < 128; i++) {
		names[i] = NULL;
		counts[i] = 0;
		whole_counts[i] = 0;
	}

	for (l = msgs; l; l = l->next) {
		gchar *content;
		gchar *name;
		gchar *p;
		gint express_found;

		express_found = 0;
		msg = (wechat_msg *) (l->data);
		if (strcmp (msg->head[HEAD_MSG_TYPE], "表情") == 0) {
			express_found == 1;
		} else {
			content = msg->content;
			if (content) {
				p = strchr (content, '[');
				if (p) {
					express_found = 1;
				}
			}
		}
		found = 0;
		name = msg->head[HEAD_NAME];
		for (i = 0; i < tails; i++) {
			if (strcmp (names[i], name) == 0) {
				found = 1;
				break;
			}
		}
			
		if (!found) {
			names[tails] = g_strdup (name);
			i = tails;
			tails ++;
		}
		whole_counts [i] ++;
		if (express_found)
			counts[i]++;
	}

	for (i = 0; i < tails; i++) {
/* talkive: 至少每天说一句话*/
		if (whole_counts[i] < 365)
			continue;
		printf ("%05f %s %05d %05d\n", 1.0 * counts[i]/whole_counts[i], names[i], counts[i], whole_counts[i]);
	}
}

static void
pic_count (GList *msgs)
{
	GList *l;
	wechat_msg *msg;

	gint tails = 0;
	gchar *names[128];
	gint counts[128];
	gint i;
	gint found;

	for (i = 0; i < 128; i++) {
		names[i] = NULL;
		counts[i] = 0;
	}

	for (l = msgs; l; l = l->next) {
		gchar *content;
		gchar *name;
		gchar *p;

		msg = (wechat_msg *) (l->data);
		if (strcmp (msg->head[HEAD_MSG_TYPE], "图片") != 0) {
			continue;
		}
		found = 0;
		name = msg->head[HEAD_NAME];
		for (i = 0; i < tails; i++) {
			if (strcmp (names[i], name) == 0) {
				found = 1;
				break;
			}
		}
			
		if (!found) {
			names[tails] = g_strdup (name);
			i = tails;
			tails ++;
		}
		counts [i] ++;
	}

	for (i = 0; i < tails; i++) {
		printf ("%05d [%s]\n", counts[i], names[i]);
	}
}

static void
pic_rate_count (GList *msgs)
{
	GList *l;
	wechat_msg *msg;

	gint tails = 0;
	gchar *names[128];
	gint counts[128];
	gint whole_counts[128];
	gint i;
	gint found;

	for (i = 0; i < 128; i++) {
		names[i] = NULL;
		counts[i] = 0;
		whole_counts[i] = 0;
	}

	for (l = msgs; l; l = l->next) {
		gchar *content;
		gchar *name;
		gchar *p;
		gint pic_found = 0;
		msg = (wechat_msg *) (l->data);
		if (strcmp (msg->head[HEAD_MSG_TYPE], "图片") == 0) {
			pic_found = 1;
		}
		found = 0;
		name = msg->head[HEAD_NAME];
		for (i = 0; i < tails; i++) {
			if (strcmp (names[i], name) == 0) {
				found = 1;
				break;
			}
		}
			
		if (!found) {
			names[tails] = g_strdup (name);
			i = tails;
			tails ++;
		}
		if (pic_found)
			counts [i] ++;
		whole_counts[i]++;
	}

	for (i = 0; i < tails; i++) {
		if (whole_counts[i]<365)
			continue;
		printf ("%05f %s %05d %05d\n", 1.0 * counts[i]/whole_counts[i], names[i], counts[i], whole_counts[i]);
	}
}


static void
link_count (GList *msgs)
{
	GList *l;
	wechat_msg *msg;

	gint tails = 0;
	gchar *names[128];
	gint counts[128];
	gint i;
	gint found;

	for (i = 0; i < 128; i++) {
		names[i] = NULL;
		counts[i] = 0;
	}

	for (l = msgs; l; l = l->next) {
		gchar *content;
		gchar *name;
		gchar *p;

		msg = (wechat_msg *) (l->data);
		if (strcmp (msg->head[HEAD_MSG_TYPE], "网页消息") != 0) {
			continue;
		}
		found = 0;
		name = msg->head[HEAD_NAME];
		for (i = 0; i < tails; i++) {
			if (strcmp (names[i], name) == 0) {
				found = 1;
				break;
			}
		}
			
		if (!found) {
			names[tails] = g_strdup (name);
			i = tails;
			tails ++;
		}
		counts [i] ++;
	}

	for (i = 0; i < tails; i++) {
		printf ("%05d [%s]\n", counts[i], names[i]);
	}
}

static void
link_rate_count (GList *msgs)
{
	GList *l;
	wechat_msg *msg;

	gint tails = 0;
	gchar *names[128];
	gint counts[128];
	gint whole_counts[128];
	gint i;
	gint found;

	for (i = 0; i < 128; i++) {
		names[i] = NULL;
		counts[i] = 0;
		whole_counts[i] = 0;
	}

	for (l = msgs; l; l = l->next) {
		gchar *content;
		gchar *name;
		gchar *p;
		gint pic_found = 0;
		msg = (wechat_msg *) (l->data);
		if (strcmp (msg->head[HEAD_MSG_TYPE], "网页消息") == 0) {
			pic_found = 1;
		}
		found = 0;
		name = msg->head[HEAD_NAME];
		for (i = 0; i < tails; i++) {
			if (strcmp (names[i], name) == 0) {
				found = 1;
				break;
			}
		}
			
		if (!found) {
			names[tails] = g_strdup (name);
			i = tails;
			tails ++;
		}
		if (pic_found)
			counts [i] ++;
		whole_counts[i]++;
	}

	for (i = 0; i < tails; i++) {
		if (whole_counts[i]<365)
			continue;
		printf ("%05f %s %05d %05d\n", 1.0 * counts[i]/whole_counts[i], names[i], counts[i], whole_counts[i]);
	}
}

static int
is_zhuanzai (char *content)
{
	/* Most possibility, zhuanzai should be long .. */
	/* 240对应大概8,9行字*/
	/* 越少越好 */
	if ((content == NULL) || (strlen (content) <240)) {
		return 0;
	}
	gchar *school[] = {"通知", "MBA", "学期", "复习", "考试", "明商",
			"明德", "明法", "教室", "题目", "作业",
			"必修", "选修", 
			"作业", "学院", "人大", "专业", 
			"13级", "12级", "14级", "班级",
			"人民大学", "老师", "同学", NULL};
	gchar *p6[] = {"P6", "六班", "p6", "生日快乐", "生快", "@", 
			"唐门", "晔", "凝", "九爷", "石头", "舰长", 
			"超人", "王超", "储超", "修楠", "张强", "马琳", NULL};
/*鸡汤一般是第三人称? */
	gchar *guess [] = {"我", NULL};
	gint i;

	for (i = 0; school[i]; i++) {
		if (strstr (content, school[i])) {
			return 0;
		}
	}
	for (i = 0; p6[i]; i++) {
		if (strstr (content, p6[i])) {
			return 0;
		}
	}
	return 1;
}

static void
whole_zhuanzai_count (GList *msgs)
{
	GList *l;
	wechat_msg *msg;

	gint link_count;
	gint zhuanzai_count;
	gint whole_count;
	gint i;

	link_count = 0;
	zhuanzai_count = 0;
	whole_count = 0;

	for (l = msgs; l; l = l->next) {
		gchar *content;
		gchar *name;
		gchar *p;
		gint zhuanzai_found;

		msg = (wechat_msg *) (l->data);

		/*统计所有*/
		whole_count ++;
		if (strcmp (msg->head[HEAD_MSG_TYPE], "网页消息") == 0) {
			link_count ++;
			continue;
		}
		if (strcmp (msg->head[HEAD_MSG_TYPE], "文字") != 0)
			continue;

		content = msg->content;
		zhuanzai_found = is_zhuanzai (content);
		if (!zhuanzai_found)
			continue;
		else
			zhuanzai_count ++;
	}

	printf ("%f  link %05d  zhuanzai %05d whole %05d\n", 1.0 * (link_count + zhuanzai_count)/whole_count,
			link_count, zhuanzai_count, whole_count);
}

static void
zhuanzai_count (GList *msgs)
{
	GList *l;
	wechat_msg *msg;

	gint tails = 0;
	gchar *names[128];
	gint counts[128];
	gint i;
	gint found;

	for (i = 0; i < 128; i++) {
		names[i] = NULL;
		counts[i] = 0;
	}

	for (l = msgs; l; l = l->next) {
		gchar *content;
		gchar *name;
		gchar *p;
		gint zhuanzai_found;

		msg = (wechat_msg *) (l->data);

		if (strcmp (msg->head[HEAD_MSG_TYPE], "文字") != 0)
			continue;

		content = msg->content;
		zhuanzai_found = is_zhuanzai (content);
		if (!zhuanzai_found)
			continue;
		found = 0;
		name = msg->head[HEAD_NAME];
		for (i = 0; i < tails; i++) {
			if (strcmp (names[i], name) == 0) {
				found = 1;
				break;
			}
		}
			
		if (!found) {
			names[tails] = g_strdup (name);
			i = tails;
			tails ++;
		}
		counts [i] ++;
	}

	for (i = 0; i < tails; i++) {
		printf ("%05d %s\n", counts[i], names[i]);
	}
}

static void 
hour_count (GList *msgs)
{
	GList *l;
	wechat_msg *msg;

	gint hour_counts [24];
	gint i;

	for (i = 0; i < 24; i++) {
		hour_counts[i] = 0;
	}

	for (l = msgs; l; l = l->next) {
		gchar *content;
		int hour;
		
		msg = (wechat_msg *) (l->data);

		content = msg->head[HEAD_TIME];
		if (!content)
			continue;

		hour = atoi (content);
		hour_counts[hour] ++;
	}

	for (i = 0; i < 24; i++) {
		printf ("%05d %02d\n", hour_counts[i], i);
	}
}

static GList *
load_name_alias (gchar *filename)
{
	char *content;
	gsize length;
        gchar **argv;
	GList *names;
	GList *l;
	int i;

	names = NULL;
        g_file_get_contents (filename, &content, &length, NULL);
        if (!content)
                return NULL;

	
        argv = g_strsplit (content, "\n", -1);
        for (i = 0; argv[i]; i++) {
		int j;
        	gchar **tmp_argv;
        	tmp_argv = g_strsplit (argv[i], " ", -1);
	        for (j = 0; tmp_argv[j]; j++) {
			name_alias *na;
			if (tmp_argv[j] && (strlen(tmp_argv[j]) > 0)) {
				na = (name_alias *) malloc (sizeof (name_alias));
				na->id = i;
				na->name = g_strdup (tmp_argv[j]);
				names = g_list_prepend (names, na);
			}
	        }
        	g_strfreev (tmp_argv);
	}

        g_strfreev (argv);
        g_free (content);

	return names;
}

static void
debug_name_alias (GList *names)
{
	GList *l;

	printf ("Debug name alias\n");
	for (l = names; l; l = l->next) {
		name_alias *na;
		na = (name_alias *) l->data;
		printf ("name <%s>, id <%d>\n", na->name, na->id);
	}
}

static int
find_id_by_name (GList *names, gchar *msg_name)
{
	GList *l;
	name_alias *na;
	if ((names == NULL) || (msg_name == NULL))
		return -1;

	for (l = names; l; l = l->next) {
		na = (name_alias *) l->data;
		if (strcmp (na->name, msg_name) == 0) {
			return na->id;
		}
	}
	return -1;
}

static char *
find_name_by_id (GList *names, gint id)
{
	GList *l;
	for (l = names; l; l = l->next) {
		name_alias *na;
		na = (name_alias *) l->data;
		if (na->id == id) {
			return na->name;
		}
	}
	return NULL;
}

static void
free_name_alias (GList *names)
{
//TODO 
	name_alias *na;
}

static void
mentioned_calculate (GList *msgs)
{
	GList *l, *l_name;
	wechat_msg *msg;
	GList *names;
	gint i, j;

	names = NULL;
	for (i = 0; i < 54; i++) {
		for (j = 0; j < 54; j++)
			mentioned_array [i][j] = 0;
	}
	names = public_name_alias;
//	debug_name_alias (names);
	for (l = msgs; l; l = l->next) {
		gchar *msg_name;
		gint msg_id;

		msg = (wechat_msg *) (l->data);
		if (msg->content == NULL)
			continue;

		msg_name = msg->head[HEAD_NAME];
		msg_id = find_id_by_name (names, msg_name);
		if (msg_id == -1)
			continue;
		/* mention someone */
		for (l_name = names; l_name; l_name = l_name->next) {
			name_alias *na;
			na = (name_alias *) l_name->data;
			/* special case: "我" should not be searched in content*/
			if (strcmp (na->name, "我") == 0)
				continue;
			if (strstr (msg->content, na->name)) {
				mentioned_array [msg_id][na->id] ++;
			}
		}
	}
	for (i = 0; i < 54; i++) {
		mentioned_count [i] = 0;
		for (j = 0; j < 54; j++) {
			if (i != j)
				mentioned_count [i] += mentioned_array [i][j];
		}
	}
	for (j = 0; j < 54; j++) {
		been_mentioned_count [j] = 0;
		for (i = 0; i < 54; i++) {
			if (i != j)
				been_mentioned_count [j] += mentioned_array [i][j];
		}
	}

	for (i = 0; i < 54; i++) {
		for (j = 0; j < 54; j++) {
			if (mentioned_count [i] == 0) {
				mentioned_rate_array[i][j] = 0.0;
			} else {
				mentioned_rate_array[i][j] = 1.0 * mentioned_array[i][j] / mentioned_count[i];
			}
		}
	}

	for (j = 0; j < 54; j++) {
		for (i = 0; i < 54; i++) {
			if (been_mentioned_count [j] == 0) {
				been_mentioned_rate_array[i][j] = 0.0;
			} else {
				been_mentioned_rate_array[i][j] = 1.0 * mentioned_array[i][j] / been_mentioned_count[j];
			}
		}
	}
#if 0
	for (i = 0; i < 54; i++) {
		for (j = 0; j < 54; j++) {
			printf ("%d\t", mentioned_array[i][j]);
		}
		printf ("\n");
	}
#endif
}

static void
certain_mentioned_name (gchar *c_name, gint all_debug)
{
	gint c_id;
	gchar *m_name;
	GList *names;

	names = public_name_alias;

	c_id = find_id_by_name (names, c_name);

	gint i;
	printf ("----- %s mentioned begin ------\n", c_name);
	for (i = 0; i < 54; i++) {
		m_name = find_name_by_id (names, i);
		if (m_name == NULL)
			continue;
		printf ("%02d %f %s\n", mentioned_array [c_id][i], mentioned_rate_array[c_id][i], m_name);

	}
	printf ("----- %s mentioned end ------\n", c_name);
}

static void
all_mentioned_name ()
{
	gint i, j;
	gchar *i_name, *j_name;
	GList *names;

	names = public_name_alias;

	printf ("----- all mentioned begin ------\n");
	for (i = 0; i < 54; i++) {
		for (j = 0; j < 54; j++) {
			if (i == j)
				continue;
			i_name = find_name_by_id (names, i);
			j_name = find_name_by_id (names, j);
			if ((i_name == NULL) || (j_name == NULL))
				continue;
			printf ("%f %s %s  %d\n", mentioned_rate_array [i][j], i_name, j_name, mentioned_array[i][j]);
		}
	}
	printf ("----- all mentioned end ------\n");
}

static void
all_been_mentioned_name ()
{
	gint i, j;
	gchar *i_name, *j_name;
	GList *names;

	names = public_name_alias;

	printf ("----- all been mentioned begin ------\n");
	for (i = 0; i < 54; i++) {
		for (j = 0; j < 54; j++) {
			if (i == j)
				continue;
			i_name = find_name_by_id (names, i);
			j_name = find_name_by_id (names, j);
			if ((i_name == NULL) || (j_name == NULL))
				continue;
			printf ("%f %s %s  %d\n", been_mentioned_rate_array [i][j], i_name, j_name, mentioned_array[i][j]);
		}
	}
	printf ("----- all been mentioned end ------\n");
}

static void
certain_been_mentioned_name (gchar *c_name, gint all_debug)
{
	gint c_id;
	gchar *m_name;
	GList *names;

	names = public_name_alias;

	c_id = find_id_by_name (names, c_name);

	gint i;
	float rate;
	printf ("----- %s been mentioned begin ------\n", c_name);
	for (i = 0; i < 54; i++) {
		m_name = find_name_by_id (names, i);
		if (m_name == NULL)
			continue;
		printf ("%02d %f %s\n", mentioned_array [i][c_id], been_mentioned_rate_array [i][c_id], m_name);

	}
	printf ("----- %s been mentioned end ------\n", c_name);
}

static int
calculate_score (gchar *msg_time, gchar *msg_part_time)
{
	gint msg_i;
	gint msg_part_i;
	gint gap;
        char *p;
	if (msg_time == NULL)
		return -1;
	if (msg_part_time == NULL)
		return -1;

        p = strchr (msg_time, ':');
	if (!p)
		return -1;
        msg_i = atoi (msg_time)*60+ atoi(p+1);

        p = strchr (msg_part_time, ':');
	if (!p)
		return -1;
        msg_part_i = atoi (msg_part_time)*60+ atoi(p+1);

	if (msg_part_i == msg_i) {
	}
	if (msg_part_i < msg_i)
		msg_part_i + 60*24;
	gap = msg_part_i - msg_i;
	if (gap <= 1) {
		/* in one minute */
		return 2;
	} else if (gap <= 5) {
		return 1;
	}

	return -1;
}

static void
timeline_calculate (GList *msgs)
{
	GList *l, *l_name;
	wechat_msg *msg;
	GList *names;
	gint i, j;

	names = NULL;
	for (i = 0; i < 54; i++) {
		for (j = 0; j < 54; j++)
			timeline_array [i][j] = 0;
	}
	names = public_name_alias;
	for (l = msgs; l; l = l->next) {
		gchar *msg_name;
		gint msg_id;
		gchar *msg_time;

		msg = (wechat_msg *) (l->data);
		if (msg->content == NULL)
			continue;

		msg_name = msg->head[HEAD_NAME];
		msg_time = msg->head[HEAD_TIME];
		msg_id = find_id_by_name (names, msg_name);
		if (msg_id == -1)
			continue;

		GList *l_part;
		wechat_msg *msg_part;
		gchar *msg_name_part;
		gint msg_id_part;
		int score_part;
		/* followed by someone */
		for (l_part = l->next; l_part; l_part = l_part->next) {
			msg_part = (wechat_msg *) l_part->data;
			msg_name_part = msg_part->head[HEAD_NAME];
			msg_id_part = find_id_by_name (names, msg_name_part);

			if (msg_id_part == -1)
				continue;
			score_part = calculate_score (msg_time, msg_part->head[HEAD_TIME]);
			if (score_part < 0) {
				break;
			} else if (msg_id == msg_id_part) {
				/* 自言自语 */
				/* 自言自语的意义是：当自言自语很多的时候，意味着这个人比较热情，有问必答
					                         少的时候，意味着简洁明了 */
				timeline_array [msg_id][msg_id_part] += score_part;
				/* break， 防止重复统计 */
				break;
			} else
				timeline_array [msg_id][msg_id_part] += score_part;
		}
	}
	for (i = 0; i < 54; i++) {
		timeline_bf_count [i] = 0;
		for (j = 0; j < 54; j++) {
			if (i == j)
				continue;
			timeline_bf_count [i] += timeline_array[i][j];
		}
	}
	for (j = 0; j < 54; j++) {
		timeline_f_count [j] = 0;
		for (i = 0; i < 54; i++) {
			if (i == j)
				continue;
			timeline_f_count [j] += timeline_array[i][j];
		}
	}


	for (i = 0; i < 54; i++) {
		for (j = 0; j < 54; j++) {
			if (timeline_bf_count [i] == 0) {
				timeline_bf_rate_array[i][j] = 0.0;
			} else {
				timeline_bf_rate_array[i][j] = 1.0 * timeline_array[i][j] / timeline_bf_count[i];
			}
		}
	}

	for (j = 0; j < 54; j++) {
		for (i = 0; i < 54; i++) {
			if (timeline_f_count [j] == 0) {
				timeline_f_rate_array[i][j] = 0.0;
			} else {
				timeline_f_rate_array[i][j] = 1.0 * timeline_array[i][j] / timeline_f_count[j];
			}
		}
	}
}

static void
certain_timeline_bf_name (gchar *c_name)
{
	gint c_id;
	gchar *m_name;
	GList *names;

	names = public_name_alias;

	c_id = find_id_by_name (names, c_name);

	gint i;
	float rate;
	printf ("----- %s been followed begin ------\n", c_name);
	for (i = 0; i < 54; i++) {
		m_name = find_name_by_id (names, i);
		if (m_name == NULL)
			continue;
		printf ("%04d %f %s\n", timeline_array [c_id][i], timeline_bf_rate_array [c_id][i], m_name);

	}
	printf ("----- %s been followed end ------\n", c_name);
}

static void
certain_timeline_f_name (gchar *c_name)
{
	gint c_id;
	gchar *m_name;
	GList *names;

	names = public_name_alias;

	c_id = find_id_by_name (names, c_name);

	gint i;
	float rate;
	printf ("----- %s follows begin ------\n", c_name);
	for (i = 0; i < 54; i++) {
		m_name = find_name_by_id (names, i);
		if (m_name == NULL)
			continue;
		printf ("%04d %f %s\n", timeline_array [i][c_id], timeline_f_rate_array [i][c_id], m_name);

	}
	printf ("----- %s follows end ------\n", c_name);
}

static void
all_timeline_bf_name ()
{
	gint i, j;
	gchar *i_name, *j_name;
	GList *names;

	names = public_name_alias;

	printf ("----- all been followed begin ------\n");
	for (i = 0; i < 54; i++) {
		for (j = 0; j < 54; j++) {
			i_name = find_name_by_id (names, i);
			j_name = find_name_by_id (names, j);
			if ((i_name == NULL) || (j_name == NULL))
				continue;
			if (i == j) {
				printf ("%f %s %s  %d - own\n", timeline_bf_rate_array [i][j], i_name, j_name, timeline_array[i][j]);
			} else
				printf ("%f %s %s  %d\n", timeline_bf_rate_array [i][j], i_name, j_name, timeline_array[i][j]);
		}
	}
	printf ("----- all been followed end ------\n");
}

static void
all_timeline_f_name ()
{
	gint i, j;
	gchar *i_name, *j_name;
	GList *names;

	names = public_name_alias;

	printf ("----- all follows begin ------\n");
	for (i = 0; i < 54; i++) {
		for (j = 0; j < 54; j++) {
			i_name = find_name_by_id (names, i);
			j_name = find_name_by_id (names, j);
			if ((i_name == NULL) || (j_name == NULL))
				continue;
			if (i == j) {
				printf ("%f %s %s  %d - own\n", timeline_f_rate_array [i][j], i_name, j_name, timeline_array[i][j]);
			} else
				printf ("%f %s %s  %d\n", timeline_f_rate_array [i][j], i_name, j_name, timeline_array[i][j]);
		}
	}
	printf ("----- all follows end ------\n");
}

static void
all_timeline_bf_star ()
{
	float star[54];

	int i, j;

	printf ("----- all been followed star begin 谁的粉丝最多 ------\n");
	for (i = 0; i < 54; i++) {
		star[i] = 0.0;
		for (j = 0; j < 54; j++) {
			if (i == j)
				continue;
			star[i] += timeline_f_rate_array[i][j];
		}
		gchar *name = find_name_by_id (public_name_alias, i);
		if (name)
			printf ("%f %s\n", star[i], name);
	}
	printf ("----- all been followed star end ------\n");
}

static void
all_timeline_f_star ()
{
	float star[54];

	int i, j;

	printf ("----- all follow others 谁是最捧场的 ------\n");
	for (j = 0; j < 54; j++) {
		star[j] = 0.0;
		for (i = 0; i < 54; i++) {
			if (i == j)
				continue;
			star[j] += timeline_bf_rate_array[i][j];
		}
		gchar *name = find_name_by_id (public_name_alias, j);
		if (name)
			printf ("%f %s\n", star[j], name);
	}
	printf ("----- all follow others  end ------\n");
}

/* old version */
static void
certain_timeline_name (gchar *c_name, int all_debug)
{
	gint s_id;
	gint s_count[3];
	gint s_i, s_loop;
	GList *names;

	names = public_name_alias;

	s_id = find_id_by_name (names, c_name);
	for (s_i = 0; s_i < 3; s_i ++) {
		s_count[s_i] = 0;
	}
	gchar *s_max_name;
	gint s_max_id = 0;
	gint s_max_count = 0;
	for (s_loop = 0; s_loop < 54; s_loop ++) {
		gint s_loop_count;
		/*不统计自己*/
		if (s_id == s_loop)
			continue;
		s_loop_count = timeline_array[s_id][s_loop];
		if (s_loop_count > s_max_count) {
			s_max_count = s_loop_count;
			s_max_id = s_loop;
		}
	}
	s_max_name = find_name_by_id (names, s_max_id); 
	printf ("%s followed by %s most, %d.  自言自语: %d\n", c_name, s_max_name, s_max_count, timeline_array[s_id][s_id]);

	s_max_id = 0;
	s_max_count = 0;
	for (s_loop = 0; s_loop < 54; s_loop ++) {
		gint s_loop_count;
		/*不统计自己*/
		if (s_id == s_loop)
			continue;
		s_loop_count = timeline_array[s_loop][s_id];
		if (s_loop_count > s_max_count) {
			s_max_count = s_loop_count;
			s_max_id = s_loop;
		}
	}
	s_max_name = find_name_by_id (names, s_max_id);
	printf ("%s talked to %s most, %d  自言自语: %d\n", c_name, s_max_name, s_max_count, timeline_array[s_id][s_id]);

	if (!all_debug)
		return;
	printf ("\n\nAll loved\n");
	for (s_loop = 0; s_loop < 54; s_loop ++) {
		gint s_loop_count;
		gchar *s_loop_name;

		s_loop_count = timeline_array[s_id][s_loop];
		if (s_loop_count < 10)
			continue;
		s_loop_name = find_name_by_id (names, s_loop);
		
		printf ("%s %d\n", s_loop_name, s_loop_count);
	}

	printf ("\n\nAll been loved\n");
	for (s_loop = 0; s_loop < 54; s_loop ++) {
		gint s_loop_count;
		gchar *s_loop_name;

		s_loop_count = timeline_array[s_loop][s_id];
		if (s_loop_count < 10)
			continue;
		s_loop_name = find_name_by_id (names, s_loop);
		
		printf ("%s %d\n", s_loop_name, s_loop_count);
	}
}

/*计算说话简洁明了*/
static void
cool_timeline_calculate ()
{
	gint max_count;
	gint max_id;
	gint gap;
	GList *names;

	names = public_name_alias;

	int i, j;

	for (i = 0; i < 54; i++) {
		max_count = 0;
		max_id = i;
		for (j = 0; j < 54; j++) {
			if (j == i)
				continue;
			if (timeline_array[i][j] > max_count) {
				max_count = timeline_array[i][j];
				max_id = j;
			}
		}
		gap = max_count - timeline_array[i][i];
		printf ("%03d %s\n", gap, find_name_by_id (names, i));
	} 
}

int main ()
{
        gchar *filename = "../data/3537003311@chatroom_20141118091245.txt.utf8-remove-system";
	GList *msgs = NULL;
	GList *l;
	wechat_msg *msg;

	public_name_alias = load_name_alias ("./name");
	msgs = load_content (filename);

//	user_count (msgs);
//	user_been_at_count (msgs);
//	day_count (msgs);
//	topic_start (msgs);
//	express_count (msgs);
//	express_rate_count (msgs);
//	pic_count (msgs);
//	pic_rate_count (msgs);
/* 就目前而言， link 比较少， 转贴比较少，都归到 whole_zhuanzai_count里面 */
//	link_count (msgs);
//	link_rate_count (msgs);
//	zhuanzai_count (msgs);
//	whole_zhuanzai_count (msgs);
//	hour_count (msgs);

	gchar *c_name;

	mentioned_calculate (msgs);
	gint cmn_all_debug = 0;
	c_name = "hopelovesboy";
	c_name = "Spring";
//	certain_mentioned_name (c_name, cmn_all_debug);
//	certain_been_mentioned_name (c_name, cmn_all_debug);
//	all_mentioned_name ();
//	all_been_mentioned_name ();
	
#if 1
	timeline_calculate (msgs);

	c_name = "hopelovesboy";
	c_name = "Spring";
//	certain_timeline_bf_name (c_name);
//	certain_timeline_f_name (c_name);
//	all_timeline_bf_name ();
//	all_timeline_f_name ();
// shoud run certain timeline first
//	cool_timeline_calculate ();

//	all_timeline_bf_star ();
	all_timeline_f_star ();
#endif

#define DEBUG_T_ALL 0
#if DEBUG_T_ALL
	int i; 
	for (i = 0; i < 54; i++) {
		gchar *ctn = find_name_by_id (public_name_alias, i);
		certain_timeline_name (ctn, 0);
	}
#endif

#ifdef WHOLE_DEBUG
	for (l = msgs; l; l = l->next) {
		msg = (wechat_msg *) (l->data);
		msg_debug (msg);
	}
#endif
	//TODO free msgs
	free_name_alias (public_name_alias);
	return 0;
}
