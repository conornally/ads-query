// gcc -Wall -g -o ads-query ads-query.c -lcurl -lcjson
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>
#include <libgen.h>
#include <sys/stat.h>

#include <curl/curl.h>
#include <cjson/cJSON.h>

struct writedata
{
	char *buf;
	size_t size;
};

struct paper
{
	cJSON *authors;
	cJSON *bibstem;
	cJSON *volume;
	cJSON *page;
	cJSON *year;
	cJSON *title;
	char bibcode[128];
};

const char *search="https://api.adsabs.harvard.edu/v1/search/query?";
const char *download="https://ui.adsabs.harvard.edu/link_gateway";
const char *fields="author,title,bibcode,bibstem,volume,page,year";
const char *export="https://api.adsabs.harvard.edu/v1/export/";

int verbose=0;
int arXiv=1;
int bibtex=0;
char output[PATH_MAX]="\0";
char *outdir;

void usage()
{
	fprintf(stderr,"usage: ads-query [-BhPvX] [-A \"abstract\" ] [-a author] [-b bibcode]\n");
	fprintf(stderr,"                 [-d doi] [-f first author] [-F \"fulltext\"] [-K \"keywords\"]\n");
	fprintf(stderr,"                 [-o outfile] [-p publication] [-r rows] [-T \"title\"]\n");
	fprintf(stderr,"                 [-t token] [-V volume] [-y year] [SEARCH STRING]\n");
	if(verbose)
	{
		fprintf(stderr,"argument summary:\n");
		fprintf(stderr,"  -A \"abstract\"   : search words in paper abstract\n");
		fprintf(stderr,"  -a author       : search paper author\n");
		fprintf(stderr,"  -B              : export bibtex file\n");
		fprintf(stderr,"  -b bibcode      : search paper bibcode\n");
		fprintf(stderr,"  -d doi          : search paper DOI\n");
		fprintf(stderr,"  -F \"fulltext\"   : search words in full text\n");
		fprintf(stderr,"  -f first author : search paper first author\n");
		fprintf(stderr,"  -K \"keywords\"   : search paper keywords\n");
		fprintf(stderr,"  -h              : print help\n");
		fprintf(stderr,"  -o output       : change output file\n");
		fprintf(stderr,"  -p publication  : search paper publication abbreviation\n");
		fprintf(stderr,"  -P              : download publicatoin version\n");
		fprintf(stderr,"  -r rows         : change result row number (default 10)\n");
		fprintf(stderr,"  -T \"title\"      : search words in paper title\n");
		fprintf(stderr,"  -t token        : NASA-ADS API token\n");
		fprintf(stderr,"  -V volume       : search paper volume\n");
		fprintf(stderr,"  -v              : verbose output\n");
		fprintf(stderr,"  -X              : download arXiv version (this option is default)\n");
		fprintf(stderr,"  -y year         : search paper year\n");
	}
	exit(1);
}

size_t write_memory_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
	size_t realsize=size*nmemb;
	struct writedata *writebuf=(struct writedata*)userp;
	char *ptr=realloc(writebuf->buf, writebuf->size+realsize+1);
	if(!ptr)
	{
		fprintf(stderr, "curl cannot realloc writedata block, no memory available.\n");
		return 0;
	}

	writebuf->buf=ptr;
	memcpy(&(writebuf->buf[writebuf->size]), contents, realsize);
	writebuf->size+=realsize;
	writebuf->buf[writebuf->size]=0;
	return realsize;
}

char *citepaper(char **dest, struct paper p)
{
	int nauthors=0;
	char *ptr, author[64], author_list[128];
	if(p.authors)
	{
		memset(author_list,'\0',128);
		nauthors=cJSON_GetArraySize(p.authors);
		for(int i=0; i<(nauthors<3?nauthors:3);i++)
		{
			strncpy(author,cJSON_GetArrayItem(p.authors,i)->valuestring, 64);
			ptr=strchr(author,',');

			strcat(author_list,ptr+2);
			strcat(author_list," ");
			strncat(author_list,author, ptr-author);
			strcat(author_list,", ");
		}
	}
	sprintf(*dest,"%s %s %s %s %s", author_list,
			(p.year?p.year->valuestring:" "),
			(p.bibstem?cJSON_GetArrayItem(p.bibstem,0)->valuestring:" "),
			(p.volume?p.volume->valuestring:" "),
			(p.page?cJSON_GetArrayItem(p.page,0)->valuestring:" ")
			);
	return *dest;
}

struct paper parsequery(cJSON *entry)
{
	struct paper p;
	p.authors=cJSON_GetObjectItemCaseSensitive(entry,"author");
	p.title=cJSON_GetObjectItemCaseSensitive(entry,"title");
	p.year=cJSON_GetObjectItemCaseSensitive(entry,"year");
	p.bibstem=cJSON_GetObjectItemCaseSensitive(entry,"bibstem");
	p.volume=cJSON_GetObjectItemCaseSensitive(entry,"volume");
	p.page=cJSON_GetObjectItemCaseSensitive(entry,"page");
	p.title=cJSON_GetObjectItemCaseSensitive(entry,"title");
	cJSON *bibcode=cJSON_GetObjectItemCaseSensitive(entry,"bibcode");
	if(bibcode) strncpy(p.bibcode,bibcode->valuestring,128);
	return p;
}

int main(int argc, char *argv[])
{
	int c,status=0;
	int rows=10;
	char q[1028], token[128], url[1028];
	char *_header;

	FILE *fp;
	//DIR *dir;

	CURL *curl=curl_easy_init();
	struct curl_slist *headers=NULL;
	struct writedata writebuf;

	cJSON *json=NULL;

	memset(q,'\0',1028);
	writebuf.buf=malloc(1);
	writebuf.size=0;

	if(argc==1) usage();
	while((c=getopt(argc,argv,"hBPvXa:A:b:d:f:F:K:p:r:V:y:t:T:o:"))!=EOF)
	{

		char _q[128];
		memset(_q,'\0',128);
		switch(c)
		{
			case 'a':snprintf(_q,128,"author:%s ",	optarg); break;
			case 'A':snprintf(_q,128,"abstract:%s ",optarg); break;
			case 'b':snprintf(_q,128,"bibcode:%s ",	optarg); break;
			case 'd':snprintf(_q,128,"doi:%s ",		optarg); break;
			case 'f':snprintf(_q,128,"author:^%s ",	optarg); break;
			case 'F':snprintf(_q,128,"full:%s ",	optarg); break;
			case 'K':printf("keywords:%s\n",optarg);snprintf(_q,128,"keyword:%s ",	optarg); break;
			case 'p':snprintf(_q,128,"bibstem:%s ",	optarg); break;
			case 'T':snprintf(_q,128,"title:%s ",	optarg); break;
			case 'V':snprintf(_q,128,"volume:%s ",	optarg); break;
			case 'y':snprintf(_q,128,"year:%s ",	optarg); break;

			case 'h':verbose++;usage();break;
			case 'v':verbose++; break;

			case 'r':rows=atoi(optarg);break;
			case 't':strncpy(token,optarg, 128); break;

			case 'B':bibtex=1;break;
			case 'P':arXiv=0;break;
			case 'X':arXiv=1;break;
			case 'o':strncpy(output,optarg,PATH_MAX);break;
		}
		if(_q[0]) strcat(q,_q);
	}
	argv+=optind;
	argc-=optind;

	while(argc)
	{
		strcat(q,argv[0]);
		strcat(q," ");
		argv++;
		argc--;
	}

	// build query string from options
	snprintf(url, 1028, "%sq=%s&fl=%s&rows=%d", search, curl_easy_escape(curl,q,0), fields,rows);

	if(!token[0])
	{
		char *tok=getenv("NASAADSTOKEN");
		if(!tok)
		{
			fprintf(stderr, "No NASAADS API token supplied.\n");
			exit(1);
		}
		strncpy(token,tok,128);
	}

	_header=malloc(1028);
	snprintf(_header,1028,"Authorization: Bearer %s",token);

	headers=curl_slist_append(headers,_header);
	curl_easy_setopt(curl,CURLOPT_URL,url);
	curl_easy_setopt(curl,CURLOPT_HTTPHEADER,headers);
	curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,write_memory_callback);
	curl_easy_setopt(curl,CURLOPT_WRITEDATA,(void*)&writebuf);
	if((status=curl_easy_perform(curl)))
	{
		fprintf(stderr,"ads-query failed to search papers.\n");
		exit(1);
	}
	// get results and display

	json=cJSON_Parse(writebuf.buf);
	cJSON *response=cJSON_GetObjectItemCaseSensitive(json,"response");
	cJSON *numfound=cJSON_GetObjectItemCaseSensitive(response,"numFound");
	cJSON *docs=cJSON_GetObjectItemCaseSensitive(response,"docs");
	int nentries=0;

	if(docs && (nentries=cJSON_GetArraySize(docs)))
	{
		int i=0;
		char *cite=malloc(256);
		struct paper *selected, paperlist[nentries];
		if(numfound && nentries<numfound->valueint && verbose) printf("*ads-query found %d more papers, increase [-r rows] to see more.\n",numfound->valueint-nentries);
		for(i=0;i<nentries;i++)
		{
			paperlist[i]=parsequery(cJSON_GetArrayItem(docs,i));
			printf("[%d] %s\n", i+1, citepaper(&cite,paperlist[i]));
		}
		if(nentries==1) selected=&paperlist[0];
		else if(nentries>1)
		{
			char input[8];

			printf("Do you want to download one of these papers: [1-%d/x]:",nentries);
			fgets(input,8,stdin);
			if(!strcmp(input,"x\n") || !strcmp(input,"X\n"))
			{
				fprintf(stderr, "No paper selected.\n");
				exit(1);
			}
			i=atoi(input);
			if(i>0 && i<=nentries)
			{
				selected=&paperlist[--i];
			}
			else
			{
				fprintf(stderr, "Selection out of range.\n");
				exit(1);
			}
		}

		sprintf(url,"%s/%s/%s",download,selected->bibcode,(arXiv?"EPRINT_PDF":"PUB_PDF"));

		free(writebuf.buf);
		writebuf.buf=malloc(1);
		writebuf.size=0;
		curl_easy_setopt(curl, CURLOPT_URL,url);
		curl_easy_setopt(curl,CURLOPT_HTTPHEADER,headers);
		curl_easy_setopt(curl,CURLOPT_WRITEDATA,(void*)&writebuf);
		curl_easy_setopt(curl,CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(curl,CURLOPT_USERAGENT,"Mozilla/5.0 (X11; Linux x86_64; rv:60.0) Gecko/20100101 Firefox/81.0");
		if(verbose)printf("Downloading:%s\n",url);
		if((status=curl_easy_perform(curl)))
		{
			fprintf(stderr, "ads-query failed to download paper.\n");
			exit(1);
		}

		if(!output[0]) snprintf(output,PATH_MAX, "%s.pdf", selected->bibcode);

		if(!(fp=fopen(output,"w")))
		{
			perror(output);
			exit(1);
		}
		fwrite(writebuf.buf,writebuf.size,1,fp);
		fclose(fp);



		//BIBTEX
		char *post_data=malloc(1028);
		if(verbose)printf("Downloading:bibtex\n");
		snprintf(post_data,1028,"{\"bibcode\":[\"%s\"]}",selected->bibcode);
		snprintf(url,1028,"%sbibtex",export);

		free(writebuf.buf);
		writebuf.buf=malloc(1);
		writebuf.size=0;
		curl_easy_setopt(curl, CURLOPT_URL,url);
		curl_easy_setopt(curl,CURLOPT_HTTPHEADER,headers);
		curl_easy_setopt(curl,CURLOPT_WRITEDATA,(void*)&writebuf);
		curl_easy_setopt(curl,CURLOPT_POST,1L);
		curl_easy_setopt(curl,CURLOPT_POSTFIELDS,post_data);
		curl_easy_perform(curl);

		json=cJSON_Parse(writebuf.buf);
		cJSON *export=cJSON_GetObjectItemCaseSensitive(json,"export");

		if(!(fp=fopen("bibtex.bib","w")))
		{
			perror("bibtext.bib");
			exit(1);
		}
		fputs(export->valuestring,fp);
		fclose(fp);

	}
	else
	{
		fprintf(stderr, "No papers found :(\n");
		exit(1);
	}

	return 0;
}
