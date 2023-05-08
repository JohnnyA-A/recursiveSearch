#define _GNU_SOURCE         // для get_current_dir_name
#define _XOPEN_SOURCE 500   // для nftw
#define UNUSED(x) (void)(x)     // для игнорирования неиспользованных функций
//подключаем необходимые библиотеки
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <dlfcn.h>
#include <ftw.h>
#include <dirent.h>
#include <unistd.h>
#include <getopt.h>


// подключаем plagin_a для работы с нашими плагинами
#include "plugin_api.h"

// структура с информацией о флагах -A -O -N 
struct def_opts {     
    unsigned int NOT;
    unsigned int AND;
    unsigned int OR;
};

// структура информации об плагине
struct plug_INFO {
    struct plugin_option *opts; // массив опций плагина
    char *name;	                // имя плагина
    int opts_len;               // количество опций плагина
    void *dl;                   // обработчик плагина
};


// определение расширения файла
const char *define_file_extension(const char *filename) {   
    const char *fe = strrchr(filename, '.');   
    if(!fe || fe==filename) return "";   
    return fe + 1;
}



struct def_opts aon_commands = {0, 0, 0};
struct plug_INFO *all_libs; // массив с информацией о всех плагинах
struct option *long_opts; // массив всех длинных опций
char *plugins_path;  // директория с плагинами
int libs_count = 0; // количество библиотек
int args_count = 0; // количество длинных опций, введенных пользователем
char **input_args = NULL; // аргументы длинных опций, введенных пользователем
int *long_opt_index= NULL; // массив индексов длинных опций в массиве long_opts в порядке введения их пользователем
int *dl_index; // массив обработчиков плагинов
int isPset = 0; // флаг, обозначающий наличие опции -P в передаваемой строке


int found_file(const char*, const struct stat*, int, struct FTW*); // функция, обрабатывающая отдельный найденный файл
void correct_exit_func(); // функция для очистки памяти перед завершением


int main(int argc, char *argv[]) {
    if (argc == 1) {                   // если нам не передали аргументов
        fprintf(stderr, "Usage: %s <write directory>\nFor help enter flag -h\n", argv[0]);
        return 1;
    }
    
    char *search_dir = ""; // директория, в которой проверяются файлы
    plugins_path = get_current_dir_name(); // получаем текущую директорию, т.е. директория плагинов по умолчанию
    atexit(&correct_exit_func); // для корректного завершения (очищения памяти)

    // обработка опций -P, -h -v
    for(int i=0; i < argc; i++) {
        
        if (strcmp(argv[i], "-P") == 0) { // обрабатываем опцию -P
            if (isPset) { // если опция -P задана повторно
                fprintf(stderr, "ERROR: the option cannot be repeated -- 'P'\n");
                return 1;
            }
            if (i == (argc - 1)) { // если за опцией -P аргумента не последует
                fprintf(stderr, "ERROR: option 'P' needs an argument\n");
                return 1;
            }	
            DIR *d;// проверяем валидность введенной директории
            if ((d=opendir(argv[i+1])) == NULL) { 
                perror(argv[i+1]); // если директория не открылаь
                 return 1;
            } 
            else {
            	closedir(d);
            	free(plugins_path); // чистим, т.к. внутри функции get_current_dir_name была выделена память
                plugins_path = argv[i+1]; // меняем директорию на ту, что ввел пользователь			
                isPset = 1;
            }   
        }


        else if (strcmp(argv[i],"-v") == 0){  // выводим иноформацию по опции -v
            fprintf(stdout, "Версия 1.0.4\nЛабораторная работа №1.\nВыполнил: Азатжонов Акбаржон, группа N3248. \nВариант: 19.\n");
            free(plugins_path);
                return 0;
        }

        else if (strcmp(argv[i],"-h") == 0) { // выводим иноформацию о всех опциях
            fprintf(stdout, "-P <dir>  Задать каталог с плагинами.\n");
            fprintf(stdout, "-A        Объединение опций плагинов с помощью операции «И» (действует по умолчанию).\n");
            fprintf(stdout, "-O        Объединение опций плагинов с помощью операции «ИЛИ».\n");
            fprintf(stdout, "-N        Инвертирование условия поиска (после объединения опций плагинов с помощью -A или -O).\n");
            fprintf(stdout, "-v        Вывод версии программы и информации о программе (ФИО исполнителя, номер группы, номер варианта лабораторной).\n");
            fprintf(stdout, "-h        Вывод справки по опциям.\n");
            return 0;
        } 

    } // конец обработки опций -P, -h, -v
    

    DIR *d; // переменная под директорию
    struct dirent *dir; // переменная под файл в директории
    d = opendir(plugins_path);
    if (d != NULL)  {
        // цикл для подсчета количества плагинов
        while ((dir = readdir(d)) != NULL) { // читаем каждый файл директории в dir
            if ((dir->d_type) == 8) { // если тип файла - обычный файл
                libs_count++;   
            }
        }
        closedir(d);
    }
    else {
        perror("opendir"); // если не получилось открыть
        exit(EXIT_FAILURE);
    }

    // выделяем память под все библиотеки (их не более libs_count)
    all_libs = (struct plug_INFO*)malloc(libs_count*sizeof(struct plug_INFO));
    if (all_libs==0){  // если при выделении памяти произошла ошибка
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    libs_count = 0; // обнуляем счетчик, чтобы заполнять массив с нуля
    d = opendir(plugins_path);
    // открываем директорию еще раз, чтобы прочитать информацию из библиотек
    if (d != NULL) {   
        while ((dir = readdir(d)) != NULL) {
            if ((dir->d_type) == 8) {  // файлы REGULAR
                if (strcmp(define_file_extension(dir->d_name),"so")==0){ // если расширение файла == so
                    char current_filename[258]; // читаем путь к плагину и записываем в current_filename
                    snprintf(current_filename, sizeof current_filename, "%s/%s", plugins_path, dir->d_name);  
										       
                    void *dl = dlopen(current_filename, RTLD_LAZY); // открываем плагин
                    if (!dl) {  // если возникла проблема при открытии
                        fprintf(stderr, "ERROR: dlopen() failed: %s\n", dlerror());
                        continue;
                    }
                    void *func = dlsym(dl, "plugin_get_info"); // читаем функцию из плагина, получаем void указатель
                    if (!func) { // если не получилось считать информацию из плагина
                        fprintf(stderr, "ERROR: dlsym() failed: %s\n", dlerror());
                    }
                    struct plugin_info pl_inf = {0}; // создаем структуру для получения информации из плагина
                    // создаем свой тип, такой же, как и тип функции плагина, на которую указывает func
                    typedef int (*pgi_func_t)(struct plugin_info*);
                    pgi_func_t pgi_func = (pgi_func_t)func;	// приводим func к созданному типу, записываем в новую переменную

                    int ret = pgi_func(&pl_inf); // вызываем функцию плагина, т.е. читаем плагин
                    if (ret < 0) {  // если не смогли записать в ret plugin_get_info(current_filename)
                        fprintf(stderr, "ERROR: plugin_get_info() failed '%s'\n", dir->d_name);
                    }
                    // записываем считанную информацию в элемент массива
                    all_libs[libs_count].name = dir->d_name;       
                    all_libs[libs_count].opts = pl_inf.sup_opts;      
                    all_libs[libs_count].opts_len = pl_inf.sup_opts_len;
                    all_libs[libs_count].dl = dl;
                    if (getenv("LAB1DEBUG")) { // выводим дебаг информацию
       		    	    fprintf(stdout, "DEBUG: Found library:\n\tPlugin name: %s\n\tPlugin purpose: %s\n\tPlugin author: %s\n", dir->d_name, pl_inf.plugin_purpose, pl_inf.plugin_author);
                    }
                    libs_count++; // увеличиваем количество успешно считанных либ
                }
            }
        }
        closedir(d);
    }
    // заканчиваем читать плагины из директории plugins_path
 
    
    size_t opt_count = 0; // счетчик всех опций
    for(int i = 0; i < libs_count; i++) { 
        // проходимся по всем плагинам и считаем общее количество опций
        opt_count += all_libs[i].opts_len;
    }

    // выделяем память под массив всех опций
    // каждая опция в массиве - это структура типа option
    long_opts=(struct option*)malloc(opt_count*sizeof(struct option));
    if (!long_opts){
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    opt_count = 0; // обнуляем счетчик
    // проходимся по всем библиотекам и заполняем массив опций этими самыми опциями
    for(int i = 0; i < libs_count; i++) {
        for(int j = 0; j < all_libs[i].opts_len; j++) {
            long_opts[opt_count] = all_libs[i].opts[j].opt;
            opt_count++; 
        }
    }

    // проверка введенных длинных опций на существование в добавленных плагинах
    int flag;
    for(int i=0; i < argc; i++) {
        if (strstr(argv[i], "--")) { // определяем длинную опцию
            flag = 0;
            for(size_t j=0; j<opt_count; j++) {
                if (strcmp(&argv[i][2], long_opts[j].name) == 0) {
                    flag = 1; // ищем имя опции среди всех опций массива long_opts
                }
            }
            // если не нашли, то даем ошибку
            if (flag == 0) {
                fprintf(stderr, "ERROR: option '%s' is not found in libs\n", argv[i]);
                return 1;
            }
        }
    }
    //конец проверки длинных опций


    // используаем getopt для простой обработки входных данных
    int c; // в этой переменной getopt возвращает короткую опцию, если нашел таковую
    int is_sdir_set = 0; // флаг, обозначающий была ли введена исследуемая директория
    int optindex = -1; // если getopt нашел длинную опцию, то в этой переменной он вернет индекс этой опции в массиве long_opts
    while((c = getopt_long(argc, argv, "-P:AON", long_opts, &optindex))!=-1) {
        // в getopt передаем строку с короткими опциями, массив длинных опций и переменную optindex
        switch(c) {
            case 'O':
                if (!aon_commands.OR) {
                    if (!aon_commands.AND) { // данные проверки для предотвращения повторов опции O
                        aon_commands.OR = 1;
                    } 
                    else { // если введены оба флага A и O, то даем ошибку
                        fprintf(stderr, "ERROR: can be either 'A' or 'O' \n");
                        return 1;
                    } 
                } 
                else {
                    fprintf(stderr, "ERROR: the option 'O' can't be repeated\n");
                    return 1;
                }
                break;
            case 'A':
                if (!aon_commands.AND) {
                    if (!aon_commands.OR) {// данные проверки для предотвращения повторов опции A
                        aon_commands.AND = 1;
                    } 
                    else {
                        fprintf(stderr, "ERROR: can be either 'A' or 'O'\n");
                        return 1;
                    }
                } 
                else {
                    fprintf(stderr, "ERROR: the option 'A' cannot be repeated\n");
                    return 1;
                }
                break;
            case 'N':
                if (!aon_commands.NOT){
                    aon_commands.NOT = 1;// данные проверки для предотвращения повторов опции N
                }
                else{
                    fprintf(stderr, "ERROR: the option 'N' cannot 'N'be repeated\n");
                    return 1;
                }
                break;
            case 'P':
                // уже обрабатывалось, поэтому пропускаем
            	break;
            case ':':
                // попадает сюда, если у короткой опции пропущен аргумент, т.е. ошибка
                return 1;
            case '?':
                // попадает сюда, если ввели короткую опцию неправильно, т.е. ошибка
                return 1;
            default: // попадает сюда, если ввели не коротку опцию
                if(optindex != -1){ // если optindex не -1, то была найдена длинная опция
                    args_count++; // прибавляем счетчик введенных длинных опций, т.к. найдена новая во входной строке
                    if (getenv("LAB1DEBUG")) {
                        fprintf(stdout, "DEBUG: Found option '%s' with argument: '%s'\n", long_opts[optindex].name, optarg);
                    }
                    // т.к. найдена новая опция, нужно увеличить размер массивов
                    long_opt_index = (int*) realloc(long_opt_index, args_count * sizeof(int));
                    if (!long_opt_index){ // если возникли проблемы с выделением памяти
                        perror("realloc");
                        exit(EXIT_FAILURE);
                    }
                    input_args = (char **) realloc(input_args, args_count * sizeof(char *));
                    if (!input_args){
                        perror("realloc");
                        exit(EXIT_FAILURE);
                    }		 

                    input_args[args_count - 1] = optarg; // записываем аргумент опции в массив input_args
                    // когда optarg находит длинную опцию, то он возвращает ее индекс в массиве long_opts через переменную optindex
                    long_opt_index[args_count - 1] = optindex; // записываем opindex в массив с индексами
                    optindex = -1;
                } 
                else { // optindex == -1, значит это должен быть путь к исследуемой директории
                    if (is_sdir_set) { // если директория была установлена уже, то повторно установить нельзя
                        fprintf(stderr, "ERROR: the examine directory has already been set at '%s'\n", search_dir );
                        return 1;
                    }
                    // если не была установлена, то проверяем валидность директории
                    // аргумент передается во внешней переменной optarg 
                    if ((d = opendir(optarg))== NULL) {    
                        perror(optarg);
                        return 1;
                    } 
                    else {
                        // если валидная, то записываем ее и поднимаем флаг
                        search_dir  = optarg;  
                        is_sdir_set = 1;
                        closedir(d);
                    }
                }
        } 
    }
    //конец обработки


    // ввод исследуемой директории обязателен, поэтому тут эта проверка
    if (strcmp(search_dir , "") == 0) {
        fprintf(stderr, "ERROR: The directory for research isn't specified\n");
        return 1;
    }
    if (getenv("LAB1DEBUG")) {
        fprintf(stdout, "DEBUG: Directory for research: %s\n", search_dir );
    }

    // если не был установлен ни флаг A, ни флаг O, то ставим значение по умолчанию ( флаг А )
    if ((aon_commands.AND==0) && (aon_commands.OR==0) ) { 
        aon_commands.AND = 1;		
    }
    
    if (getenv("LAB1DEBUG")) {
        fprintf(stdout, "DEBUG: Information about input aon_commandss:\n\tAND: %d\tOR: %d\tNOT: %d\n", aon_commands.AND, aon_commands.OR, aon_commands.NOT);
    }
    /* т.к. пользователь ввел опции в произвольном порядке, создаем массив с обработчиками библиотек в том же порядке,
       в котором пользователь вводил опции; в этом массиве вполне могут быть повторяющиеся элементы*/
    dl_index = (int *)malloc(args_count*sizeof(int));
    if (!dl_index){ // если возникла проблема при выделении памяти
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    // определяем, из какой библиотеки была опция
    for(int i=0; i < args_count; i++) {
        // проходимся по всем введенным опциям
        const char *opt_name = long_opts[long_opt_index[i]].name; // записываем имя текущей опции в переменную
        for(int j=0; j < libs_count; j++) { 
            for(int k=0; k < all_libs[j].opts_len; k++) { // проходимся по всем опциям каждой библиотеки
                if (strcmp(opt_name, all_libs[j].opts[k].opt.name) == 0) {
                    // ищем индекс библиотеки в массиве all_libs для текущей опции opt_name
                    dl_index[i] = j; // записываем индекс найденной либы в массив индексов обработчиков
                    
                }
            }
        }
    }
	
    if (getenv("LAB1DEBUG")) {
        fprintf(stdout, "DEBUG: Directory with libraries: %s\n", plugins_path);
    }
    // функцию обхода директории
    int res = nftw(search_dir , found_file, 20, FTW_PHYS || FTW_DEPTH);
    if (res < 0) { // если возникла ошибка при инициализации пробега
        perror("nftw");
        return 1;
    }
    return 0;

}
   
typedef int (*proc_func_t)( const char *name, struct option in_opts[], size_t in_opts_len);
int found_file(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    if (typeflag == FTW_F) {   // если файл - обычный файл, то обрабатываем его
        int rescondition = aon_commands.NOT ^ aon_commands.AND; // устанавливаем начальное значение в зависимости от флагов A O N
        for (int i = 0; i < args_count; i++) { // проходимся по все опциям, т.к. файл нужно проверить на все опции
            struct option opt = long_opts[long_opt_index[i]]; // получаем текущую опцию
            char * arg = input_args[i];
            if (arg) { // определяем ее аргумент, если он есть
                opt.has_arg = 1;
                opt.flag = (void *)arg;
            } else {
                // если нет, то так и пишем
                opt.has_arg = 0;
            }

            // открываем нужную функцию нужного плагина, с помощью заранее созданного массив dl_index
    	    void *func = dlsym(all_libs[dl_index[i]].dl, "plugin_process_file");
            proc_func_t proc_func = (proc_func_t)func; // приводим указатель на функцию к нужному типу
            int res_func;
            res_func = proc_func(fpath, &opt, 1); // вызываем функцию плагина для проверки файла


            // нормализуем res_func так, чтобы
            // 1 = плагин вернул положительный результат
            // 0 = плагин вернул отрицательный результат
            if (res_func) {
                if (res_func > 0) {
                    res_func = 0;
                } 
                else {
                    fprintf(stderr, "Plugin execution error\n");
                    return 1;
                }
            } 
            else {
                res_func = 1;
            }

            // для простого объяснения просто рассмотреть все случаи (их всего 4)
            if (aon_commands.NOT ^ aon_commands.AND) {
                if (!(rescondition = rescondition & (aon_commands.NOT ^ res_func)))
                    break;
            } else {
                if ((rescondition = rescondition | (aon_commands.NOT ^ res_func)))
                    break;
            }
        }

        // выводим путь к файлу, если подошел
        if (rescondition) {
            fprintf(stdout, "%s\n",fpath);
        }
        else{
            if (getenv("LAB1DEBUG")) {
                fprintf(stdout, "DEBUG: File: %s doesn't match the search criteria\n", fpath);
            }
        }
    }	
    		
    UNUSED(sb); // чтобы не ругался valgrind 
    UNUSED(ftwbuf);
    return 0;
}


// функция чистки памяти
void correct_exit_func() {
    for (int i=0;i<libs_count;i++){
        if (all_libs[i].dl) dlclose(all_libs[i].dl);
    }
    if (all_libs) free(all_libs);
    if (dl_index) free(dl_index);
    if (long_opts) free(long_opts);
    if (long_opt_index) free(long_opt_index);
    if (input_args) free(input_args);
    if (!isPset) free(plugins_path);
}


