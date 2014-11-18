#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

typedef struct _user_count_info {
	gchar *name;
	gint count;
} user_count_info;

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

int main ()
{
        gchar *filename = "../data/3537003311@chatroom_20141118091245.txt.utf8-remove-system";
	GList *msgs = NULL;
	GList *l;
	wechat_msg *msg;

	msgs = load_content (filename);

	user_count (msgs);

#ifdef WHOLE_DEBUG
	for (l = msgs; l; l = l->next) {
		msg = (wechat_msg *) (l->data);
		msg_debug (msg);
	}
#endif
	return 0;
}
