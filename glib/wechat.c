#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

gchar *public_name [] = {

};

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

/* who start most topic with reply */
static void
topic_start (GList *msgs)
{
	GList *l;
	wechat_msg *msg;

	gint new_tails = 0;
	gint end_tails = 0;
	gchar *new_names[128];
	gint new_counts[128];
	gchar *end_names[128];
	gint end_counts[128];
	gint i;
	gint found;
	wechat_msg *last_msg = NULL;

	for (i = 0; i < 128; i++) {
		new_names[i] = NULL;
		new_counts[i] = 0;
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
int main ()
{
        gchar *filename = "../data/3537003311@chatroom_20141118091245.txt.utf8-remove-system";
	GList *msgs = NULL;
	GList *l;
	wechat_msg *msg;

	msgs = load_content (filename);

//	user_count (msgs);
//	user_been_at_count (msgs);
//	day_count (msgs);
//	topic_start (msgs);
//	express_count (msgs);
	express_rate_count (msgs);

#ifdef WHOLE_DEBUG
	for (l = msgs; l; l = l->next) {
		msg = (wechat_msg *) (l->data);
		msg_debug (msg);
	}
#endif
	return 0;
}
