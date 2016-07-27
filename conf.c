#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include "cJSON.h"
#include "conf.h"

#define UPGRADECMD "upgradecmd"
#define PROGRAMS "programs"
#define NAME "name"
#define VERSION "version"
#define DOWNLOADNAME "downloadname"
#define UPDATECMD "updatecmd"
#define RUNCMD "runcmd"

#define PAGESIZE 512

void copy(char * src, char * dst){
	FILE *in_fd = fopen(src,"rb"); 
	if(!in_fd) {
		perror("in_fd");
		return;
	}
	FILE *out_fd = fopen(dst,"wb"); 
	if(!out_fd) {
		perror("out_fd");
		return;
	}

	if(-1 == access(dst, W_OK)) {
		perror("access");
		return;
	}
	
	char buf[PAGESIZE];
	size_t n = 0;

	while (n != EOF) {
		n = fread(buf, 1, PAGESIZE, in_fd);
		if (n == 0)
			break;
		fwrite(buf, 1, n, out_fd);
	}
	fclose(in_fd);
	fclose(out_fd);
}

struct conf * loadconf(cJSON *json){
	if(!json){
		return NULL;
	}
	cJSON *jprograms= cJSON_GetObjectItem(json,PROGRAMS);
	int programcount = cJSON_GetArraySize(jprograms); 

	struct program * programs = (struct program *)malloc(sizeof(struct program)*programcount);
	memset(programs, 0, sizeof(struct program) * programcount); 

	cJSON * name;
	cJSON * version;
	cJSON * downloadname;
	cJSON * updatecmd;
	cJSON * runcmd;

	int i = 0;
	for(;i<programcount; i++){ 
		cJSON * subitem = cJSON_GetArrayItem(jprograms, i);
		name = cJSON_GetObjectItem(subitem, NAME);
		if(name && name->type == cJSON_String){
			programs[i].name = strdup(name->valuestring);
		}
		version = cJSON_GetObjectItem(subitem, VERSION);
		if(version && version->type == cJSON_String){
			programs[i].version = atoi(version->valuestring);
		}
		downloadname = cJSON_GetObjectItem(subitem, DOWNLOADNAME);
		if(version && version->type == cJSON_String){
			programs[i].downloadname = strdup(downloadname->valuestring);
		} 
		updatecmd  = cJSON_GetObjectItem(subitem, UPDATECMD);
		if(updatecmd && updatecmd->type == cJSON_String){
			programs[i].updatecmd = strdup(updatecmd->valuestring);
		}
		runcmd = cJSON_GetObjectItem(subitem, RUNCMD);
		if(runcmd && runcmd->type == cJSON_String){
			programs[i].runcmd = strdup(runcmd->valuestring);
		}
	}

	cJSON * jupgradecmd = cJSON_GetObjectItem(json, UPGRADECMD);
	struct conf * conf = (struct conf *) malloc(sizeof(struct conf));
	memset(conf, 0, sizeof(struct conf));
	if(jupgradecmd && jupgradecmd->type == cJSON_String){
		conf->upgradecmd = strdup(jupgradecmd->valuestring);
	}else{
		return NULL;
	}
	conf->programcount = programcount;
	conf->programs = programs;

	return conf;
}

int includeprogram(struct conf * conflocal, struct program * program){
	int i ;
	for(i = 0; i < conflocal->programcount; i++){
		if(strlen(conflocal->programs[i].name) == strlen(program->name) && 
		   0 == strcmp(conflocal->programs[i].name, program->name) &&
		   conflocal->programs[i].version == program->version){
			return 1;
		}
	}

	return 0;
}

struct list_head * getconf(struct conf * conflocal, struct conf * confurl){ 
	if(conflocal == NULL || confurl == NULL){
		return NULL;
	} 

	struct list_head * head = (struct list_head *)malloc(sizeof(struct list_head));
	INIT_LIST_HEAD(head);

	int i;
	for(i = 0; i < confurl->programcount; i++){
		if(!includeprogram(conflocal, &confurl->programs[i])){ 
			list_add_tail(&confurl->programs[i].list, head);
		}
	}

	return head;
}

void destroyprogram( struct program * program ){
	if(program){
		free(program->name);
		free(program->updatecmd);
		free(program->runcmd);
	}
}

void destroyconf(struct conf * conf){ 
	if(conf){
		int i = 0;
		for(; i < conf->programcount; i++){
			destroyprogram(&conf->programs[i]);
		}
		free(conf->upgradecmd);
		free(conf->programs);
	}
	free(conf);
}

void downloadprogram(struct list_head * head)
{
	struct program * program;

	struct list_head * pos, *n;
	list_for_each_safe(pos, n, head){ 
		program = list_entry(pos, struct program, list); 
		remove(program->downloadname);
		if (0 == system(program->updatecmd)) {
			printf("copy %s to %s...\n", program->downloadname, program->name);
			copy(program->downloadname, program->name); 
			int rt = remove(program->downloadname);
			if(rt)
				fprintf(stdout, "%d %s\n", rt, strerror(errno));
			system(program->runcmd);
		}
		//system(program->runcmd);
	}
}

void runprograms(struct conf * urlconf){
	int i;
	for(i = 0; i < urlconf->programcount; i++){ 
		system(urlconf->programs[i].runcmd);
	}
}

