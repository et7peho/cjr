/* Console Jenkins rss display
 *
 * Copyright (c) 2016- Peter Holmberg <peter.holmberg@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */



#include <stdio.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <string.h>
#include <curl/curl.h>
#include <unistd.h>
#include <getopt.h>

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"
#define ANSI_CLEAR_TERM    "\x1b[2J"
#define URL_MAX 128
#ifdef DEBUG
#define _d(M, ...) fprintf(stdout, "DEBUG %s:%d: " M "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#define __LOG_FILE__ "/tmp/rss"
#define log_file(M, ...) { FILE *fp; fp=fopen(__LOG_FILE__,"a"); if(fp) fprintf(fp, "[%s:%d:] " M "\n", __FILE__, __LINE__, ##__VA_ARGS__); fclose(fp); }
#else
#define _d(M, ...)
#define log_file(M, ...)
#endif
#define _e(M, ...) fprintf(stderr, "ERROR %s:%d: " M "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#define _enn(M, ...) fprintf(stderr, "ERROR %s:%d: " M, __FILE__, __LINE__, ##__VA_ARGS__)

struct string
{
  char *c;
  size_t s;
};

static int verbose_flag; // Not used yet
void print_version()
{
  printf("Version: %s\n", VERSION);
}
void print_job(char *job)
{
  char *buff;
  char *cId;
  char *cId_end;
  int iId;

  cId = strchr (job, '#')+1;

  if(!cId)
    return;
  buff = malloc(8); //Make valgrind happy should be a char buff[8];
  memset(buff,0,8);
  cId_end=strchr (cId,' ');

  strncpy ( buff, cId, cId_end-cId);

  iId = strtol(buff, (char **)NULL, 10);
  cId_end++;
  if(strstr(cId_end , "broken since this build"))
    printf(ANSI_COLOR_RED "%d\t%s"     ANSI_COLOR_RESET "\n", iId, cId_end);
  if(strstr(cId_end , "broken since build"))
    printf(ANSI_COLOR_MAGENTA "%d\t%s"     ANSI_COLOR_RESET "\n", iId, cId_end);
  if(strstr(cId_end , "back to normal"))
    printf(ANSI_COLOR_GREEN "%d\t%s"     ANSI_COLOR_RESET "\n", iId, cId_end);
  if(strstr(cId_end , "stable"))
    printf(ANSI_COLOR_GREEN "%d\t%s"     ANSI_COLOR_RESET "\n", iId, cId_end);
  free(buff);

}

void print_element_names(xmlNode * a_node, char *needle, int i)
{
  xmlNode *cur_node = NULL;

  for (cur_node = a_node; cur_node; cur_node = cur_node->next)
  {
    if (cur_node->type == XML_ELEMENT_NODE)
    {
      if(strncmp((char*)cur_node->name, needle ,strlen(needle))==0)
      {
        if(i)
        {
          print_job((char*)cur_node->children->content+1);
        }
        else
        {
          printf("\n\n===%s===\n",(char*)cur_node->children->content);
          printf("Id\tStatus\n");
          i++;
        }
      }
    }

    print_element_names(cur_node->children, needle, i);
  }

}

int parse_xml(struct string *buff)
{
  xmlDocPtr doc;
  xmlNode *root_element = NULL;
  doc = xmlReadMemory (buff->c , buff->s, "noname.xml", NULL, 0);
  if (doc == NULL)
  {
    return 1;
  }

  root_element = xmlDocGetRootElement(doc);

  print_element_names(root_element, "title", 0);

  xmlFreeDoc(doc);
  xmlCleanupParser();
  return 0;
}


size_t curl_write( void *ptr, size_t size, size_t nmemb, void *void_buff)
{
  struct string *buff = (struct string*) void_buff;
  int n=0;
  n = buff->s - ( strlen (buff->c) + (size*nmemb));
  if (n < 0 )
  {
    n*=-1;
    buff->c = realloc(buff->c ,buff->s + n+1); //+1 for \0
    buff->s+=n+1;
  }
  strncat(buff->c, (char*)ptr ,size * nmemb);
  return size*nmemb;
}

int getrssfile(char *url, struct string *buff)
{
  CURLcode status;
  CURL *curl = curl_easy_init();
  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)buff);
  status =curl_easy_perform(curl);
  curl_easy_cleanup(curl);
  return status;
}


int main (int argc, char *argv[])
{
  int curlstatus=0;
  int c;
  int timeout=60;
  char url[URL_MAX+1];
  struct string buff;
  int i=0;
  unsigned int loops=0;
  while (1)
  {
    static struct option long_options[] =
    {
      {"verbose", no_argument,       &verbose_flag, 1},
      {"brief",   no_argument,       &verbose_flag, 0},
      {"version",   no_argument, 0, 0},

      {"time",  required_argument, 0, 't'},
      {"loops",  required_argument, 0, 'l'},
      {"url",  required_argument, 0, 'u'},
      {0, 0, 0, 0}
    };

    int option_index = 0;

    c = getopt_long (argc, argv, "vt:u:l:", long_options, &option_index);

    if (c == -1)
      break;

    switch (c)
    {
      case 0:
        /* If this option set a flag, do nothing else now. */
        if (long_options[option_index].flag != 0)
          break;
        printf ("option %s", long_options[option_index].name);
        if (optarg)
          printf (" with arg %s", optarg);
        printf ("\n");
        break;

      case 't':
        timeout = (int)strtol(optarg, (char **)NULL, 10);
        if(timeout<1)
        {
          _e("Timeout to smal setting to default 60 sec. Given argument is %s Confirm with return key", optarg);
          timeout=60;
          getc(stdin);
        }
        break;

      case 'l':
        loops = (int)strtol(optarg, (char **)NULL, 10);
        break;

      case 'v':
        print_version();
        return 0;
        break;

      case 'u':
        strncpy(url, optarg, URL_MAX);
        break;

      case '?':
        /* getopt_long already printed an error message. */
        break;

      default:
        abort ();
    }
  }
  if(strncmp(url,"http",4)) // Should be a real validation
  {
    _e("Missing url or bad url %s", url);
    return 1;
  }

  if (optind < argc)
  {
    printf ("non-option ARGV-elements: ");
    while (optind < argc)
      printf ("%s ", argv[optind++]);
    putchar ('\n');
    return 1;
  }



  _d();
  curl_global_init(CURL_GLOBAL_ALL ^ CURL_GLOBAL_SSL);
  for(;;)
  {
    _d();
    buff.s =100;
    buff.c =malloc(100);
    buff.c[0]='\0';
    if(curlstatus)
    {
      for(; c>=0; c--)
        fprintf(stderr,"\r");
    }
    curlstatus = getrssfile(url, &buff);
    if ( curlstatus)
    {
      c = _enn("Error %s with url%s", curl_easy_strerror(curlstatus), url);
    }
    else
    {
      _d();
      printf(ANSI_CLEAR_TERM);
      printf("Url:%s timeout=%d", url, timeout);
      if(loops)
        printf("%d/%d", i, loops);


      if (parse_xml ( &buff) ==0)
      {
        _d();
        free (buff.c);
        buff.s=0;
      }
      _d();
    }
    sleep (timeout);
    _d();
    if (loops > 0 )
    {
      i++;
      if ( i >= loops )
      {
        break;
      }
    }
  }
  xmlMemoryDump();
  return(0);
}
