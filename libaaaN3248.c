// Библиотека, позволяющаю искать ipv4 в бинарном виде
// Ищет заданную последовательность в двочиной форме 
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include "plugin_api.h" 

// Данные о нашей библиотеке 
static char *m_lib_name = "libaaaN3248.so";
static char *m_plugin_purpose = "Look for binary ipv4 in files";
static char *m_plugin_author = "Azatjonov Akbarjon Azizjon ugli, N3248";
//Объявляю функцию проверки
int check_ip(unsigned char*, unsigned int, unsigned int, unsigned int);

// Структура, описывающая опцию, поддерживаемую плагином
struct plugin_option my_option_description[] = {
	{{"ipv4-addr-bin", 1, 0, 0},     	// имя нашей опции; нужен аргумент; флаг = 0, т.е. возвращается val; и, собственно, сам val
	 "Look for given ipv4 address in binary form in files "}
};


size_t my_all_options_len = sizeof(my_option_description) / sizeof(my_option_description[0]);  // соответсвенно длина вышестоящего списка
// Функция, позволяющая получить информацию о плагине
int plugin_get_info(struct plugin_info* ppi) { 
    if (!ppi) { 	// если информацию не удаётся получить, дальше работать с плагином нельзя, возвращаем ошибку
        fprintf(stderr, "ERROR: invalid argument\n");
        return -1;
    }
    // если все хорошо, показываем наши данные
    ppi->plugin_purpose = m_plugin_purpose;
    ppi->plugin_author = m_plugin_author;
    ppi->sup_opts_len = my_all_options_len;  // длина списка опций
    ppi->sup_opts = my_option_description;	// список опций
    return 0;
}


// Фунция, позволяющая выяснить, отвечает ли файл заданным критериям. Возвращает 0, если проверка прошла, 1, если нет и -1 при ошибке
int plugin_process_file(const char *fname, struct option in_opts[], size_t in_opts_len){
	char *DEBUG = getenv("LAB1DEBUG");
	// Сначала проверим, дали ли нам опцию:
	if (in_opts_len <= 0) {
		fprintf(stderr, "ERROR: option is not given\n");
		return -1;
	}

	// Проверим данный нам файл
	if (!fname) {
		fprintf(stderr, "ERROR: filename is not given\n");
		return -1;
	}
	// Укажем для режима отладки данные, которые здесь получены
	if (DEBUG) {
        for (size_t i = 0; i < in_opts_len; i++) {
            fprintf(stderr, "DEBUG: %s: Got option '%s' with arg '%s'\n",
                m_lib_name, in_opts[i].name, (char*)in_opts[i].flag);
        }
    }
 	// Так как у нас одна опция и она принимает только 1 флаг, нам достаточно рассмотреть только 1 случай
 	char *opt_flag  = (char *)in_opts[0].flag;
	// Далее проверим формат нашего флага 
	int tmp = 0; // здесь проходит проверка на количетсво точек в строке
	for (size_t i = 0; i < strlen(opt_flag); i++) if (opt_flag[i] == '.') tmp ++;
	if (tmp != 3){
	 	fprintf(stderr, "ERROR: ipv4 in wrong format (d.d.d.d needed)\n");
		return -1;
		}

	//Проверим, все ли символы являются цифрами
	tmp = 0;// здесь счетчик увеличивается, если символ не является цифрой или точкой
	for (size_t i = 0; i < strlen(opt_flag); i++) if (! (opt_flag[i] == '.' || (opt_flag[i] >= '0' && opt_flag[i] <= '9')) ) tmp++;
	if (tmp) {
		fprintf(stderr, "ERROR: ipv4 in wrong format (only numbers and dots can be used)\n");
		return -1;
		}

	// Делим ip на 4 части и переводим в int
	int p[4] = {0, 0, 0, 0}; tmp = 0; int on_ch = 0;
	for (size_t i = 0; i < strlen(opt_flag); i++){
		if (opt_flag[i] >= '0' && opt_flag[i] <= '9'){
			on_ch = 10 * on_ch + (opt_flag[i] - 48);
		}
		else{ 
			p[tmp++] = on_ch;	
			on_ch = 0;
		}
	}
	p[tmp] = on_ch;
 	if  ((p[0] > 255) || (p[1] > 255) || (p[2] > 255) || (p[3] > 255)) { // и сразу же проверяем, чтобы все числа были правильные
		fprintf(stderr, "ERROR: ipv4 in wrong format (all numbers must be between 0 and 255)\n");
		return -1;
		}
    
    
    // Получим наши ip в little-endian и  bin-endian
    unsigned int ip_big_end =    p[0] * 256 * 256 * 256 +  p[1] * 256 * 256 + p[2] * 256 + p[3];
    unsigned int ip_little_end = p[3] * 256 * 256 * 256 +  p[2] * 256 * 256 + p[1] * 256 + p[0];
//    printf("%x\n %x", ip_little_end, ip_big_end);//		192.168.8.1   C0 A8 08 01
    
    
    // Наконец откроем наш файл
    int fd = open(fname, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "ERROR:Error with file opening\n");
        return -1;
    }

    // Проверим размер файла (если он меньше 4 байт, то искать нам нет смысла)
    struct stat st = {0};
    int res = fstat(fd, &st);
    if (res < 0) {
        fprintf(stderr, "ERROR: Error with file opening\n");
                if (DEBUG) {
            fprintf(stderr, "DEBUG: %s: fstat error\n",
                m_lib_name);
        }
        close(fd);
        return -1;
    }
    if (st.st_size < 4) {
        if (DEBUG) {
            fprintf(stderr, "DEBUG: %s: File size should be >= 4\n",
                m_lib_name);
        }
        close(fd);
        return 1;
    }
    
    // получаем наши данные с файла 
    unsigned char *ptr = mmap(0, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (ptr == MAP_FAILED) {
        fprintf(stderr, "ERROR: mmap error\n");
        close(fd);
    
    }
        
    // Проверяем, есть ли наш ip  в файле:
    tmp = 0;
    tmp = check_ip(ptr,st.st_size, ip_big_end, ip_little_end);
    if (tmp) {
        munmap(ptr, st.st_size);
        close(fd);
        if (DEBUG) {
            if (tmp == 1) fprintf(stdout, "DEBUG: %s: The file countains ipv4 address in big-endian form\n", m_lib_name);
            if (tmp == 2) fprintf(stdout, "DEBUG: %s: The file countains ipv4 address in little-endian form\n", m_lib_name);
        }
        return 0;
    }

    munmap(ptr, st.st_size);
    close(fd);
	return 1;
}

int check_ip(unsigned char* block, unsigned int len, unsigned int ip_big_end, unsigned int ip_little_end){
    long long test = 0;
    int ret = 0;
    for(size_t i = 0; i < len - 3; i++){
        test ^= test;
        memcpy(&test, block + i, 4);
        if( (test ^ ip_big_end) == 0){
            ret = 1;
        }
        if( (test ^ ip_little_end) == 0){
            ret = 2;
        }
    }
    return ret;
}
