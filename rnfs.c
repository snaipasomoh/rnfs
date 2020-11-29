//gcc rnfs.c -o rnfs `pkg-config fuse3 --cflags --libs`
#define FUSE_USE_VERSION 39

#include <fuse3/fuse.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <assert.h>

char *IDS = NULL;

const size_t SECS = 10; //sets secs in minute to speed up app

static void init_ids ();

static uint gen_id ();

static void free_id (uint id);

static void destroy_ids ();

struct{
	int live_males;
	int live_females;
	int adult_males;
	int adult_females;
	int dead_males;
	int dead_females;
	char *content;
} statistics;

static void make_content (){
	char res[256];
	char buf[32];
	free(statistics.content);
	sprintf(buf, "%d live males\n", statistics.live_males);
	strcpy(res, buf);
	sprintf(buf, "%d live females\n", statistics.live_females);
	strcat(res, buf);
	sprintf(buf, "%d adult males\n", statistics.adult_males);
	strcat(res, buf);
	sprintf(buf, "%d adult females\n", statistics.adult_females);
	strcat(res, buf);
	sprintf(buf, "%d dead males\n", statistics.dead_males);
	strcat(res, buf);
	sprintf(buf, "%d dead females\n", statistics.dead_females);
	strcat(res, buf);
	statistics.content = strdup(res);
}

static void init_stat (){
	statistics.live_males = 0;
	statistics.live_females = 0;
	statistics.adult_males = 0;
	statistics.adult_females = 0;
	statistics.dead_males = 0;
	statistics.dead_females = 0;
	make_content();
}

typedef struct Rabbit{
	uint id;
	// 0 - male, 1 - female
	int sex;
	int pregnant;
	time_t birth_time;
	// inited as birth_time + SECS * 2; for females updates after every birth
	time_t next_child_time;
} Rabbit;

static Rabbit make_rabbit (time_t birth_time, uint id){
	static int rand_inited = 0;
	if (!rand_inited){
		srand(time(NULL));
		rand_inited = 1;
	}

	Rabbit new_rabbit;
	new_rabbit.id = id ? id : gen_id();
	IDS[new_rabbit.id] = 1; //kostyl' ebaniy no mne poh
	new_rabbit.sex = rand() % 2;
	new_rabbit.pregnant = 0;
	new_rabbit.birth_time = birth_time;
	new_rabbit.next_child_time = birth_time + SECS * 2;

	return new_rabbit;
}

typedef struct Cage{
	Rabbit *rabbits;
	int size;
	int *positions;
} Cage;

static Cage make_cage (){
	Cage new_cage;
	new_cage.size = 0;
	new_cage.rabbits = malloc(sizeof(Rabbit) * 10);
	memset(new_cage.rabbits, 0, sizeof(Rabbit) * 10);
	new_cage.positions = malloc(sizeof(int) * 10);
	memset(new_cage.positions, 0, sizeof(int) * 10);
	return new_cage;
}

static void add_rabbit_in_cage (Rabbit rabbit, Cage *cage){
	int pos;
	if (cage->size == 10){
		pos = rand() % 10;
	}
	else{
		for (int i = 0; i < 10; i++){
			if (!cage->positions[i]){
				pos = i;
				break;
			}
		}
	}
	if (cage->positions[pos]){
		if (cage->rabbits[pos].sex){
			statistics.dead_females++;
		}
		else{
			statistics.dead_males++;
		}
		free_id(cage->rabbits[pos].id);
	}
	cage->rabbits[pos] = rabbit;
	cage->positions[pos] = 1;
	cage->size = cage->size < 10 ? cage->size + 1 : 10;
}

static int check_rabbit_in_cage (uint id, Cage *cage){
	int pos = -1;
	for (int i = 0; i < 10; i++){
		if (cage->positions[i] && cage->rabbits[i].id == id){
			pos = i;
			break;
		}
	}
	if (pos == -1){
		return 0;
	}
	return 1;
}

static Rabbit get_rabbit_from_cage (uint id, Cage *cage){
	Rabbit res;
	res.id = 0;
	int pos = -1;
	for (int i = 0; i < 10; i++){
		if (cage->positions[i] && cage->rabbits[i].id == id){
			pos = i;
			break;
		}
	}
	if (pos == -1){
		return res;
	}
	cage->positions[pos] = 0;
	cage->size--;
	return cage->rabbits[pos];
}

static void kill_rabbit_in_cage (uint id, Cage *cage){
	int pos = -1;
	for (int i = 0; i < 10; i++){
		if (cage->positions[i] && cage->rabbits[i].id == id){
			pos = i;
			break;
		}
	}
	if (pos == -1){
		return;
	}
	if (cage->rabbits[pos].sex){
		statistics.dead_females++;
	}
	else{
		statistics.dead_males++;
	}
	free_id(cage->rabbits[pos].id);
	cage->positions[pos] = 0;
	cage->size--;
}

static void delete_cage (Cage *cage){
	free(cage->rabbits);
	free(cage->positions);
}

struct{
	int capacity;
	Cage *cages;
} nursery;

static void init_nursery (int capacity){
	nursery.capacity = capacity;
	nursery.cages = malloc(sizeof(Cage) * capacity);
	for (int i = 0; i < capacity; i++){
		nursery.cages[i] = make_cage();
	}
} 

static void delete_nursery (){
	for (int i = 0; i < nursery.capacity; i++){
		delete_cage(nursery.cages + i);
	}
	free(nursery.cages);
}

static void init_ids (){
	free(IDS);
	IDS = malloc(sizeof(char) * nursery.capacity * 13);
	IDS[0] = 1;
}

static uint gen_id (){
	for (uint i = 0; i < nursery.capacity * 13; i++){
		if (!IDS[i]){
			IDS[i] = 1;
			return i;
		}
	}
	return 0;
}

static void free_id (uint id){
	if (id >= nursery.capacity * 13){
		return;
	}
	IDS[id] = 0;
}

static void destroy_ids (){
	free(IDS);
}

static void update_state (){
	time_t curr_time = time(NULL);
	int live_males = 0;
	int live_females = 0;
	int adult_males = 0;
	int adult_females = 0;
	for (int i = 0; i < nursery.capacity; i++){
		Cage *cage = nursery.cages + i;
		while (1){
			time_t least_next_child_time = curr_time;
			int mother_pos = -1;
			for (int j = 0; j < 10; j++){
				if (cage->positions[j] && cage->rabbits[j].sex &&
				    cage->rabbits[j].next_child_time < least_next_child_time){
					least_next_child_time = cage->rabbits[j].next_child_time;
					mother_pos = j;
				}
			}
			if (mother_pos == -1){
				break;
			}
			uint mother_id = cage->rabbits[mother_pos].id;
			time_t mother_nct = cage->rabbits[mother_pos].next_child_time;
			add_rabbit_in_cage(make_rabbit(least_next_child_time, 0), cage);
			add_rabbit_in_cage(make_rabbit(least_next_child_time, 0), cage);
			if (cage->rabbits[mother_pos].id != mother_id ||
			    cage->rabbits[mother_pos].next_child_time != mother_nct){
				break;
			}
			cage->rabbits[mother_pos].pregnant = 0;
			for (int j = 0; j < 10; j++){
				if (cage->positions[j] && !cage->rabbits[j].sex &&
				    cage->rabbits[j].next_child_time <= least_next_child_time){
					cage->rabbits[mother_pos].pregnant = 1;
					break;
				}
			}
			cage->rabbits[mother_pos].next_child_time += SECS;
		}
		for (int j = 0; j < 10; j++){
			if (cage->positions[j]){
				if (cage->rabbits[j].sex){
					live_females++;
					if (cage->rabbits[j].birth_time + SECS * 2 < curr_time){
						adult_females++;
					}
				}
				else{
					live_males++;
					if (cage->rabbits[j].birth_time + SECS * 2 < curr_time){
						adult_males++;
					}
				}
			}
		}
	}
	statistics.live_males = live_males;
	statistics.live_females = live_females;
	statistics.adult_males = adult_males;
	statistics.adult_females = adult_females;
	make_content();
}

static void *rnfs_init (struct fuse_conn_info *conn, struct fuse_config *cfg){
	(void) conn;
	cfg->kernel_cache = 1;
	return NULL;
}

static int rnfs_getattr (const char *path, struct stat *st,
                         struct fuse_file_info *fi){
	(void) fi;
	memset(st, 0, sizeof(struct stat));
	st->st_uid = getuid();
	st->st_gid = getgid();
	st->st_atime = time(NULL);
	st->st_mtime = time(NULL);
	char end = 0;
	char sex = 0;
	int cage = 0;
	uint id = 0;
	if (strcmp(path, "/") == 0){
		st->st_mode = S_IFDIR | 0755;
		st->st_nlink = 2;
	}
	else if (strcmp(path, "/stat") == 0){
		st->st_mode = S_IFREG | 0444;
		st->st_nlink = 1;
		st->st_size = strlen(statistics.content);
	}
	else if (sscanf(path, "/cage%d%c", &cage, &end) == 1){
		if (cage < 1 || cage > nursery.capacity){
			return -ENOENT;
		}
		st->st_mode = S_IFDIR | 0555;
		st->st_nlink = 2;
	}
	else if (sscanf(path, "/cage%d/%u.%c%c", &cage, &id, &sex, &end) == 3){
		if (cage < 1 || cage > nursery.capacity){
			return -ENOENT;
		}
		if (!check_rabbit_in_cage(id, &nursery.cages[cage - 1])){
			return -ENOENT;
		}
		st->st_mode = S_IFREG | 0222;
		st->st_nlink = 1;
	}
	else{
		return -ENOENT;
	}
	return 0;
}

static int rnfs_readdir (const char *path, void *buffer, fuse_fill_dir_t filler,
                         off_t offset, struct fuse_file_info *fi,
                         enum fuse_readdir_flags flags){
	(void) offset;
	(void) fi;
	(void) flags;
	int root = 0;
	int cagen = 0;

	if (strcmp(path, "/") == 0){
		root = 1;
	}
	else if (strstr(path, "/cage") == path){
		sscanf(path, "/cage%d", &cagen);
	}
	else{
		return -ENOENT;
	}

	filler(buffer, ".", NULL, 0, 0);
	filler(buffer, "..", NULL, 0, 0);

	update_state();

	char buf[32];
	if (root){
		for (int i = 0; i < nursery.capacity; i++){
			sprintf(buf, "cage%d", i + 1);
			filler(buffer, buf, NULL, 0, 0);
		}
		filler(buffer, "stat", NULL, 0, 0);
	}
	else{
		Cage cage = nursery.cages[cagen - 1];
		for (int i = 0; i < 10; i++){
			if (cage.positions[i]){
				sprintf(buf, "%u.%c", cage.rabbits[i].id,
				        cage.rabbits[i].sex ? 'f' : 'm');
				filler(buffer, buf, NULL, 0, 0);
			}
		}
	}
	
	return 0;
}

static int rnfs_read (const char *path, char *buffer, size_t size, off_t offset,
                      struct fuse_file_info *fi){
	(void) fi;
	size_t len;
	char *cnt;
	if (strcmp(path, "/stat") == 0){
		update_state();
		len = strlen(statistics.content);
		cnt = statistics.content;
	}
	else{
		return -ENOENT;
	}

	if (offset < len){
		if (offset + size > len){
			size = len - offset;
		}
		memcpy(buffer, cnt + offset, size);
	}
	else{
		size = 0;
	}
	return size;
}

static int rnfs_unlink (const char *path){
	int cage = 0;
	uint id = 0;
	char sex = 0;
	char end = 0;
	if (sscanf(path, "/cage%d/%u.%c%c", &cage, &id, &sex, &end) != 3){
		return -EPERM;
	}
	if (cage < 1 || cage > nursery.capacity){
		return -ENOENT;
	}
	if (id < 1 || id >= nursery.capacity * 13 || !IDS[id]){
		return -ENOENT;
	}
	if (sex != 'f' && sex != 'm'){
		return -ENOENT;
	}
	kill_rabbit_in_cage(id, &nursery.cages[cage - 1]);

	return 0;
}

static int rnfs_utimens (const char *path, const struct timespec ts[2],
                       struct fuse_file_info *fi){
	return 0;
}

static int rnfs_create (const char *path, mode_t mode,
                        struct fuse_file_info *fi){
	int cage = 0;
	uint id = 0;
	char sex = 0;
	char end = 0;
	if (sscanf(path, "/cage%d/%u.%c%c", &cage, &id, &sex, &end) != 3){
		return -EPERM;
	}
	if (cage < 1 || cage > nursery.capacity){
		return -EPERM;
	}
	if (id < 1 || id >= nursery.capacity * 13 || IDS[id]){
		return -EPERM;
	}
	if (sex != 'f' && sex != 'm'){
		return -EPERM;
	}
	Rabbit new_rabbit = make_rabbit(time(NULL), id);
	new_rabbit.sex = sex == 'm' ? 0 : 1;
	add_rabbit_in_cage(new_rabbit, &nursery.cages[cage - 1]);
	return 0;
}

static int rnfs_rename (const char *src, const char *dst, unsigned int flags){
	int cage1 = 0;
	uint id1 = 0;
	char sex1 = 0;
	int cage2 = 0;
	uint id2 = 0;
	char sex2 = 0;
	char end = 0;
	if (sscanf(src, "/cage%d/%u.%c%c", &cage1, &id1, &sex1, &end) != 3){
		return -EPERM;
	}
	if (cage1 < 1 || cage1 > nursery.capacity){
		return -EPERM;
	}
	if (id1 < 1 || id1 >= nursery.capacity * 13 || !IDS[id1]){
		return -EPERM;
	}
	if (sex1 != 'f' && sex1 != 'm'){
		return -EPERM;
	}
	if (sscanf(dst, "/cage%d/%u.%c%c", &cage2, &id2, &sex2, &end) != 3){
		return -EPERM;
	}
	if (cage1 < 1 || cage2 > nursery.capacity){
		return -EPERM;
	}
	if (id2 < 1 || id2 >= nursery.capacity * 13 || !IDS[id1]){
		return -EPERM;
	}
	if (sex2 != 'f' && sex2 != 'm'){
		return -EPERM;
	}
	if (id1 != id2){
		return -EPERM;
	}
	if (sex1 != sex2){
		return -EPERM;
	}
	if (cage1 == cage2){
		return 0;
	}

	add_rabbit_in_cage(get_rabbit_from_cage(id1, &nursery.cages[cage1 - 1]),
	                   &nursery.cages[cage2 - 1]);

	return 0;
}

static struct fuse_operations rn_operations = {
	.init = rnfs_init,
	.getattr = rnfs_getattr,
	.readdir = rnfs_readdir,
	.utimens = rnfs_utimens,
	// .open = rnfs_open,
	.read = rnfs_read,
	// .mknod = rnfs_mknod,
	.unlink = rnfs_unlink,
	.create = rnfs_create,
	.rename = rnfs_rename,
};

int main (int argc, char **argv){
	if (argc < 2){
		printf("To few args\n");
		return -1;
	}

	int cages = atoi(argv[1]);
	if (cages <= 0){
		printf("Wrong CAGES value\n");
		return -1;
	}
	init_nursery(cages);
	init_stat();
	init_ids();

	char **cor_argv = malloc(sizeof(*cor_argv) * (argc - 1));
	cor_argv[0] = argv[0];
	for (int i = 2; i < argc; i++){
		cor_argv[i - 1] = argv[i];
	}
	int ret = fuse_main(argc - 1, cor_argv, &rn_operations, NULL);
	free(cor_argv);
	destroy_ids();
	delete_nursery();
	free(statistics.content);
}