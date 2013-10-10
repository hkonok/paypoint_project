

/*****************************************************************************/
/*** paypoint payment server                                               ***/
/*****************************************************************************/
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <string>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <resolv.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <queue>
#include <semaphore.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <linux/types.h>
#include <ctime>
#include <execinfo.h>
#include <curl/curl.h>
#include <my_global.h>
#include <mysql.h>

void PANIC(char* msg);
#define PANIC(msg)  { perror(msg); exit(-1); }
#define MESSAGE_SIZE 1024
#define IP "127.0.0.1"
#define PORT 9337
#define CONN_LIMIT 5
#define MYSQL_USER "root"
#define MYSQL_PASS "root123"
#define MYSQL_IP "localhost"
#define MYSQL_DB_NAME "prepago"
#define HEATING_METER_URL "http://162.13.37.69/meter_control/clear_shutoff.php?customer="
using namespace std;

unsigned char payment_or_reversal[5] = "0200";
unsigned char response_to_payment_or_reversal[5] = "0210";
unsigned char void_of_payment_or_reversal[5] = "0420";
unsigned char response_to_void[5] = "0430";
unsigned char processing_code_payment[7] = "000000";
unsigned char processing_code_reversal[7] = "220000";

//for testing 
unsigned char response_processing_code[7] = "00";
//for testing

/*
 * semaphores for sending and receiving queue
 */
sem_t sem_receive, sem_send, sem_curl, sem_exit;
/*
 * mutex lock for queues
 */
pthread_mutex_t mut_rcv = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mut_send = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mut_curl = PTHREAD_MUTEX_INITIALIZER;

/*
 * threads for reading, processing, sending and accepting function
 */
pthread_t read_t, process_t, send_t, accept_t, curl_t;

int start_reading = 0, start_processing = 0, start_sending = 0, start_connection = 0;

/*
 * contains full message
 */

/*
 * log file
 */


class msg_class{
	public:
		unsigned char *msg;
		int len;
		int client;
	msg_class(unsigned char *ptr,int msg_len){
		msg = ptr;
		len = msg_len;
	}
	msg_class(unsigned char *ptr,int msg_len,int old_client){
		msg = ptr;
		len = msg_len;
		client = old_client;
	}
	msg_class(){
	}
	void free_msg(){
        delete[] msg;
		len = 0;
	}
};

/*
 * contains parsed conponet of message
 */
class msg_content{
	public:
	int type;
	vector<unsigned char> v;
};

/*
 * contains all parsed components of a message
 */
class parsed_msg{
	public:
	unsigned char mti[4];
	__u64 bm1,bm2;
	vector<msg_content> mc;
};

/*
 * all receiving messages are saved in this queue
 */
queue<msg_class> receive_queue;

/*
 * response messages saved after processing in this queue
 */

queue<msg_class> send_queue;

/*
 * curl messages for sending
 */
queue<string> curl_message;

/*
 * message component parcer function
 */

msg_class function_payment_message(parsed_msg &pmsg, double balance, double arrears, bool is_balance_needed, bool is_reversal);
msg_class function_void_message(parsed_msg &pmsg);


/*
 * function for stack trace
 */

void handler(int sig) {
  void *array[200];
  size_t size;

  // get void*'s for all entries on the stack
  size = backtrace(array, 50);

  // print out all the frames to stderr
  fprintf(stderr, "Error: signal %d:\n", sig);
  backtrace_symbols_fd(array, size, STDERR_FILENO);
  exit(1);
}


 msg_content msg_parcing(int type,unsigned char *msg){

     cout<<"now type: "<<type<<endl;

	msg_content mc;
	int i = -1,j;
	mc.type = type;
	switch(type){
		case 2:
			i = (int)(msg[0] - '0')*10 + (int)(msg[1] - '0');
			i += 2;
			break;
		case 3: 
			i = 6;
			break;
		case 4: 
			i = 12;
			break;
		case 7: 
			i = 10;
			break;
		case 11: 
			i = 6;
			break;	
		case 12:
			i = 6;
			break;

		case 13:
			i = 4;
			break;

		case 14:
			i = 4;
			break;
		case 15:
			i = 4;
			break;
		case 18:
			i = 4;
            break;
		case 22:
			i = 3;
			break;
		case 25:
			i = 2;
			break;
		case 28:
			i = 9;
			break;
		case 30:
			i = 9;
			break;
		case 32:
			i = (int)(msg[0] - '0')*10 + (int)(msg[1] - '0');
			i += 2;
			break;
		case 33:
			i = (int)(msg[0] - '0')*10 + (int)(msg[1] - '0');
			i += 2;
			break;
		case 35:
			i = (int)(msg[0] - '0')*10 + (int)(msg[1] - '0');
			i += 2;
			break;
		case 37:
			i = 12;
			break;
		case 39:
			i = 2;
			break;
		case 41:
			i = 8;
			break;
		case 42:
			i = 15;
			break;
		case 43:
			i = 40;
			break;
		case 48:
			i = (int)(msg[0] - '0')*100 + (int)(msg[1] - '0')*10 + (int)(msg[2] - '0');
			i += 3;
			break;
		case 49:
			i = 3;
			break;
		case 54:
			i = (int)(msg[0] - '0')*100 + (int)(msg[1] - '0')*10 + (int)(msg[2] - '0');
			i += 3;
			break;
		case 56:
			i = (int)(msg[0] - '0')*100 + (int)(msg[1] - '0')*10 + (int)(msg[2] - '0');
			i += 3;
			break;
		case 59:
			i = (int)(msg[0] - '0')*100 + (int)(msg[1] - '0')*10 + (int)(msg[2] - '0');
			i += 3;
			break;
		case 90:
			i = 42;
			break;
		case 95:
			i = 42;
			break;
		case 123:
			i = (int)(msg[0] - '0')*100 + (int)(msg[1] - '0')*10 + (int)(msg[2] - '0');
			i += 3;
			break;
		case 127:
			i = (int)(msg[0] - '0')*100000 + (int)(msg[1] - '0')*10000 + (int)(msg[2] - '0')*1000 + 
			    (int)(msg[3] - '0')*100 + (int)(msg[4] - '0')*10 + (int)(msg[5] - '0');
			i += 6;
			break;
	}
    cout<<"segment length: "<<i<<endl;
    for(j = 0; j < i; j++){
		mc.v.push_back(msg[j]);
        cout<<msg[j];
    }
    cout<<endl;
	return mc;
 }
parsed_msg my_msg_parcer(msg_class &tmp){
	parsed_msg pm;
    //tmps
    unsigned char myu_char;
    //end tmps
	int i, j, crnt = 0;
	__u64 my_var;
    __u64 tmp_bmp1, tmp_bmp2, process_bmp1, process_bmp2;
	// mti parsing 
	for(i = 0, crnt = 2; i < 4; i++, crnt++)
		pm.mti[i] = tmp.msg[crnt];

    //1st bit map
    for(i = 0, tmp_bmp1 = 0; i< 8; i++, crnt++){
        tmp_bmp1 = (tmp_bmp1 << 8) | (__u64)(tmp.msg[crnt]);
    }

    for(i = 0, pm.bm1 = (__u64)0, process_bmp1 = (__u64)1 << 63; i<64 ; i++, process_bmp1 = process_bmp1>>1){
        pm.bm1 = (__u64)pm.bm1 >> 1;
        if(tmp_bmp1 & process_bmp1){
            pm.bm1 = pm.bm1 | ((__u64)1<<63);
            cout<<"1";
        }
        else{
            cout<<"0";
        }
    }
    cout<<endl;
	
    if(pm.bm1 & (__u64)1<<58)
        cout<<"pm.bm1 59 is found"<<endl;

    //2nd bit map
    if( pm.bm1 & (__u64)1){
        cout<<"2nd bit map found"<<endl;

        for(i = 0, tmp_bmp2 = 0; i < 8; i++, crnt++)
            tmp_bmp2 = (tmp_bmp2 << 8) | (__u64)(tmp.msg[crnt]);

        for(i = 0, pm.bm2 = 0, process_bmp2 = (__u64)1 << 63; i<64 ; i++, process_bmp2 = process_bmp2>>1){
            pm.bm2 = pm.bm2 >> 1;
            if(tmp_bmp2 & process_bmp2){
                pm.bm2 = pm.bm2 | (__u64)1<<63;
            }

        }

	}
	else {
        cout<<"2nd bit map not found"<<endl;
		pm.bm2 = 0;
	}
    for( i = 2, my_var = (__u64)2; i <= 64 ; i++, my_var = my_var << 1){
		if(pm.bm1 & my_var){
            cout<<"crnt: "<<crnt<<"first char: "<<tmp.msg[crnt]<<endl;
			msg_content ob_content= msg_parcing(i, &tmp.msg[crnt]);
			crnt += ob_content.v.size();
			pm.mc.push_back(ob_content);
		}
	}
	
    if(pm.bm1 & (__u64)1){
		for(i = 65, my_var = 1; i <= 128 ; i++, my_var = my_var << 1)
			if(pm.bm2 & my_var){
				msg_content ob_content = msg_parcing(i, &tmp.msg[crnt]);
				crnt += ob_content.v.size();
				pm.mc.push_back(ob_content);
			}
	}
	return pm;
}

/*
 * process curl request
 */

void* process_curl(void* arg){
    /*
     * CURL init
     */
    CURL *curl;
    CURLcode res;
    curl = curl_easy_init();
    char str[200];
    string tmp_message;
    while(true){
        sem_wait(&sem_curl);

        pthread_mutex_lock(&mut_curl);
        tmp_message = curl_message.front();
        curl_message.pop();
        pthread_mutex_unlock(&mut_curl);
        cout<<"PROCESS_CURL: "<<tmp_message<<endl;
        if(tmp_message == "exit")
        {
            curl_easy_cleanup(curl);
            sem_post(&sem_exit);
            return arg;
        }
        sprintf(str, "%s", tmp_message.c_str());
        if(curl){
            curl_easy_setopt(curl, CURLOPT_URL, str);
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            res = curl_easy_perform(curl);
            if(res != CURLE_OK){
              fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            }
        }
        else{
            curl = curl_easy_init();
        }
        printf("curl operation successfull\n");
    }

    return arg;
}


/*
 * all in coming messages are read from this function
 */

void* read_message(void* arg){
    signal(SIGSEGV, handler);


	int client = *(int *) arg;
	int msg_len, i;
	unsigned int next_msg_len;
    unsigned char *msg;
    printf("strting read_message\n");

	while(start_reading){

        try{
                msg = new unsigned char[MESSAGE_SIZE];
                msg_len = recv(client, msg, 2, MSG_WAITALL);

                /*
                 * handeling error code 11
                 */
                if(msg_len != 2){
                    throw 11;
                }

                next_msg_len = 0;
                for(i = 0; i < 2; i++)
                    next_msg_len = ((next_msg_len << 8)|msg[i]);

                printf("next msg len %d\n",next_msg_len);

                /*
                 * handeling error code 12
                 */
                if(next_msg_len <= 0 || next_msg_len> 900){
                    throw 12;
                }


                msg_len = recv(client, &msg[2], next_msg_len, MSG_WAITALL);
                msg_len += 2;

                /*
                 * handeling error code 13
                 */
                if(msg_len != next_msg_len + 2 || next_msg_len == 0){
                    throw 13;
                }

                msg[msg_len] = 0;
                printf("Message: %s\n",&msg[2]);
                printf("LEN: %d\n",msg_len);
                pthread_mutex_lock(&mut_rcv);
                receive_queue.push(msg_class(msg, msg_len, client));
                pthread_mutex_unlock(&mut_rcv);
                sem_post(&sem_receive);

        }
        catch(int e){
            printf("\nan error has occurd. ERROR CODE %d\n", e);
            delete[] msg;
            close(client);
            return arg;
        }
		
	}

    delete[] msg;
    close(client);
	printf("ending read_message\n");
	return arg;
}

/*
 * message are parsed from receiving queue in this function
 */
void* process_message(void *arg){
    signal(SIGSEGV, handler);

	msg_class tmp_message;
	printf("starting process_message\n");

	msg_class payment_message_ob;



    /*
     * mysql connection
     */
    MYSQL *con = mysql_init(NULL);
    if (con == NULL)
    {
            fprintf(stderr, "%s\n", mysql_error(con));
            exit(1);
    }

    if (mysql_real_connect(con, MYSQL_IP, MYSQL_USER, MYSQL_PASS, MYSQL_DB_NAME, 0, NULL, 0) == NULL)
    {
            fprintf(stderr, "%s\n", mysql_error(con));
            printf("error in database connection.");
            mysql_close(con);
            exit(1);
     }
    /*
     * end of mysql connection
     */


	while(start_processing){
		sem_wait(&sem_receive);

		cout<<"start processing received message."<<endl;

		pthread_mutex_lock(&mut_rcv);
		tmp_message = receive_queue.front();
		receive_queue.pop();
		pthread_mutex_unlock(&mut_rcv);
        int client = tmp_message.client;


		int crnt_number;
		char file_name[100];
	        char cwd[200];


	
		/*
 		 * process message	 
 		 */	

        /*
         * variables for database entry
         */
        bool wrong_response;
        double balance, arrears, is_balance_needed;
        char ref_number[100];
        char pan[100];
        char customer_id[50];
        char scheme_number[100];
        char time_date[50];
        char currency_code[5];
        char amount[15];
        char transsection_fee[15];
        char acceptor_name_location[50];
        char payment_received[5];
        char cancelled[5];
        char settlement_date[10];
        char marchant_type[10];
        char pos_entry_mode[5];
        char meter_id[20];
        char my_year[5], my_month[5], my_day[5], my_hour[5], my_min[5], my_sec[5];
        char query_string[500];
        int try_for_my_sql_connection;

        /*
         * end variables for database entry end
         */

		cout<<"starting parcing message."<<endl;
		parsed_msg pmsg;	
		pmsg = my_msg_parcer(tmp_message);
		tmp_message.free_msg();
        payment_message_ob.client = tmp_message.client;

		if(memcmp((unsigned char *)pmsg.mti, (unsigned char *)payment_or_reversal, 4) == 0){
            is_balance_needed = false;
            int to_find_type;
            for(to_find_type = 0; to_find_type < pmsg.mc.size(); to_find_type++)
            {
                if(pmsg.mc[to_find_type].type == 3)
                    break;
            }
            if( to_find_type < pmsg.mc.size() && pmsg.mc[to_find_type].type == 3){
				unsigned char test_message_type[7] = "000000";
                for(int i = 0; i < pmsg.mc[to_find_type].v.size(); i++)
                    test_message_type[i] = pmsg.mc[to_find_type].v[i];
				if(memcmp((unsigned char *) test_message_type, (unsigned char *) processing_code_payment, 6) == 0){
					/*
					 * payment message
					 */
                    try{
                        /*
                         * process variables for database entry
                         */

                        wrong_response = true;
                        int my_good = 1;

                        if(
                             !(
                                   (pmsg.bm1 &(__u64)1<<1)  && (pmsg.bm1 & (__u64)1<<58)
                                && (pmsg.bm1 &(__u64)1<<3)  && (pmsg.bm1 &(__u64)1<<14)
                                && (pmsg.bm1 &(__u64)1<<17) && (pmsg.bm1 &(__u64)1<<21)
                                && (pmsg.bm1 &(__u64)1<<27) && (pmsg.bm1 &(__u64)1<<36)
                                && (pmsg.bm1 &(__u64)1<<42) && (pmsg.bm1 &(__u64)1<<48)
                              )
                         )
                        {
                            sprintf((char *)response_processing_code, "01");
                            printf("my_good %d\n", my_good);
                            printf("\n\n wrong response code 01\n\n\n");
                            goto payment_message_tag;
                        }



                        sprintf((char *)response_processing_code, "00");

                        //current_date time
                        {
                            time_t now = time(0);
                            tm *ltm = localtime(&now);

                            sprintf(my_year,"%d", 1900 + ltm->tm_year);
                            sprintf(my_month, "%.2d", 1 + ltm->tm_mon);
                            sprintf(my_day, "%.2d", ltm->tm_mday);

                            sprintf(my_hour, "%.2d", ltm->tm_hour);
                            sprintf(my_min, "%.2d", ltm->tm_min);
                            sprintf(my_sec, "%.2d" , ltm->tm_sec);
                        }

                        //field 02
                        int var_type, var_help1, var_help2;
                        for(var_type = 0; var_type < pmsg.mc.size(); var_type++){
                            if(pmsg.mc[var_type].type == 2)
                                break;
                        }
                        for(var_help2 = 0, var_help1 = 2; var_help1 < pmsg.mc[var_type].v.size(); var_help2++, var_help1++){
                            pan[var_help2] = pmsg.mc[var_type].v[var_help1];
                        }
                        pan[var_help2] = 0;

                        //field 04
                        for(var_type; var_type < pmsg.mc.size(); var_type++){
                            if(pmsg.mc[var_type].type == 4)
                                break;
                        }
                        for(var_help1 = 0; var_help1 < pmsg.mc[var_type].v.size(); var_help1++){
                            amount[var_help1] = pmsg.mc[var_type].v[var_help1];
                        }
                        amount[var_help1] = 0;

                        if(amount[var_help1 - 1] != '0' || amount[var_help1 - 2] != '0'){
                            sprintf((char *)response_processing_code, "07");
                            printf("\n\n wrong response code 07\n\n\n");
                            goto payment_message_tag;
                        }

                        amount[var_help1 - 2] = 0;
                        double amount_limit_ckecker;
                        sscanf(amount, "%lf", &amount_limit_ckecker);

                        if(amount_limit_ckecker < 10.00){
                            sprintf((char *)response_processing_code, "02");
                            printf("\n\n wrong response code 02\n\n\n");
                            goto payment_message_tag;
                        }
                        if(amount_limit_ckecker > 500.00){
                            sprintf((char *)response_processing_code, "03");
                            printf("\n\n wrong response code 03\n\n\n");
                            goto payment_message_tag;
                        }


                        //field 15
                        for(var_type; var_type < pmsg.mc.size(); var_type++){
                            if(pmsg.mc[var_type].type == 15)
                                break;
                        }
                        sprintf(settlement_date,"%s-%c%c-%c%c",my_year, pmsg.mc[var_type].v[0], pmsg.mc[var_type].v[1], pmsg.mc[var_type].v[2], pmsg.mc[var_type].v[3]);

                        //field 18
                        for(var_type; var_type < pmsg.mc.size(); var_type++){
                            if(pmsg.mc[var_type].type == 18)
                                break;
                        }
                        marchant_type[0] = pmsg.mc[var_type].v[0];
                        marchant_type[1] = pmsg.mc[var_type].v[1];
                        marchant_type[2] = pmsg.mc[var_type].v[2];
                        marchant_type[3] = pmsg.mc[var_type].v[3];
                        marchant_type[4] = 0;

                        //field 22
                        for(var_type; var_type < pmsg.mc.size(); var_type++){
                            if(pmsg.mc[var_type].type == 22)
                                break;
                        }
                        pos_entry_mode[0] = pmsg.mc[var_type].v[0];
                        pos_entry_mode[1] = pmsg.mc[var_type].v[1];
                        pos_entry_mode[2] = pmsg.mc[var_type].v[2];
                        pos_entry_mode[3] = 0;

                        //field 28
                        for(var_type; var_type < pmsg.mc.size(); var_type++){
                            if(pmsg.mc[var_type].type == 28)
                                break;
                        }
                        for(var_help1 = 0; var_help1 < pmsg.mc[var_type].v.size(); var_help1++){
                            transsection_fee[var_help1] = pmsg.mc[var_type].v[var_help1];
                        }
                        transsection_fee[var_help1] = 0;

                        //modified according to the specification 2013/09/11
                        double t_fee;
                        sscanf(amount, "%lf", &t_fee);
                        sprintf(transsection_fee, "%.2lf", t_fee*0.01 + 0.3);
                        // end modified according to the specification

                        //field 37
                        for(var_type; var_type < pmsg.mc.size(); var_type++){
                            if(pmsg.mc[var_type].type == 37)
                                break;
                        }
                        for(var_help1 = 0; var_help1 < pmsg.mc[var_type].v.size(); var_help1++){
                            ref_number[var_help1] = pmsg.mc[var_type].v[var_help1];
                        }
                        ref_number[var_help1] = 0;

                        //field 43
                        for(var_type; var_type < pmsg.mc.size(); var_type++){
                            if(pmsg.mc[var_type].type == 43)
                                break;
                        }
                        for(var_help1 = 0; var_help1 < pmsg.mc[var_type].v.size(); var_help1++){
                            acceptor_name_location[var_help1] = pmsg.mc[var_type].v[var_help1];
                        }
                        acceptor_name_location[var_help1] = 0;

                        //field 49
                        for(var_type; var_type < pmsg.mc.size(); var_type++){
                            if(pmsg.mc[var_type].type == 49)
                                break;
                        }
                        currency_code[0] = pmsg.mc[var_type].v[0];
                        currency_code[1] = pmsg.mc[var_type].v[1];
                        currency_code[2] = pmsg.mc[var_type].v[2];
                        currency_code[3] = 0;
                        //customer_id & scheme_number
                        sprintf(query_string, "SELECT * FROM customers WHERE barcode='%s'", pan);

                        try_for_my_sql_connection = 0;
                        while(try_for_my_sql_connection < 3){

                            if(mysql_query(con, query_string) == 0)
                            {
                                MYSQL_RES *result = mysql_store_result(con);
                                MYSQL_ROW row;
                                while ((row = mysql_fetch_row(result))){
                                    sprintf(customer_id, "%s", row[0]);
                                    sprintf(scheme_number, "%s", row[25]);
                                    sprintf(meter_id, "%s", row[3]);
                                    sscanf(row[2],"%lf",&balance);
                                    sscanf(row[16] ,"%lf", &arrears);
                                    wrong_response = false;
                                    break;
                                }
                                mysql_free_result(result);
                                break;
                            }
                            else{
                                if(con != NULL){
                                    mysql_close(con);
                                }
                                con = mysql_init(NULL);
                                if (con == NULL)
                                {
                                   // fprintf(stderr, "%s\n", mysql_error(con));
                                   printf("error in mysql_init()\n");
									 //exit(1);
                                }

                                if (con != NULL && mysql_real_connect(con, MYSQL_IP, MYSQL_USER, MYSQL_PASS, MYSQL_DB_NAME, 0, NULL, 0) == NULL)
                                {
                                   // fprintf(stderr, "%s\n", mysql_error(con));
                                    printf("error in database connection.");
                                   // mysql_close(con);
                                    //exit(1);
                                 }
                                try_for_my_sql_connection++;
                            }
                        }

                        if(wrong_response == true){
                            sprintf((char *)response_processing_code, "04");
                            printf("\n\n wrong response code 04\n\n\n");
                            goto payment_message_tag;
                        }
                        /*
                         * end process variables for database entry
                         */


                        /*
                         * insert into mysql database
                         */
                         sprintf(query_string, "INSERT INTO temporary_payments VALUES('%s', %s, %s, '%s', '%s-%s-%s %s:%s:%s', %s,'%s', \"%s\", \"%s\", 0, 0, '%s', %s, %s )",
                                             ref_number, customer_id, scheme_number, pan, my_year, my_month, my_day, my_hour, my_min, my_sec, currency_code, amount,
                                transsection_fee, acceptor_name_location, settlement_date, marchant_type, pos_entry_mode);
                        printf("\n\n%s\n\n",query_string);
                        if (mysql_query(con, query_string))
                          {
                            printf("inser in to temporary_payments failed.\n");
                          }
                        /*
                         * end insert into mysql database
                         */

                        /*
                         * update mysql database
                         */
                        double temp_balance;
                        sscanf(amount, "%lf", &temp_balance);
                        sprintf(query_string, "UPDATE customers SET balance=%lf, last_top_up='%s-%s-%s %s:%s:%s', IOU_available=0, IOU_used=0, IOU_extra_available=0, IOU_extra_used=0, admin_IOU_in_use=0 WHERE id=%s",
                                               balance+temp_balance, my_year, my_month, my_day, my_hour, my_min, my_sec, customer_id);
                        printf("\n\n\n%s\n\n\n", query_string);
                        if (mysql_query(con, query_string))
                          {
                            printf("update to customers failed.\n");
                          }


                        if(balance + temp_balance > 0.00){
                            sprintf(query_string, "UPDATE customers SET shut_off=0, shut_off_command_sent=0, credit_warning_sent=0  WHERE id=%s", customer_id);
                            printf("\n\n\n%s\n\n\n", query_string);
                            if (mysql_query(con, query_string))
                              {
                                printf("update to customers failed.\n");
                              }
                            /* // district_heating_meters replaced by cCurl request
                            sprintf(query_string, "UPDATE district_heating_meters SET shut_off_device_status=0, scheduled_to_shut_off=0 WHERE meter_ID=%s", meter_id);
                            printf("\n\n\n%s\n\n\n", query_string);
                            if (mysql_query(con, query_string))
                              {
                                printf("update to district_heating_meters failed.\n");
                              }
                              */

                              // http://162.13.37.69/meter_control/clear_shutoff.php?customer=2
                              char heating_meter_url[200];
                              string curl_tmp;
                              sprintf(heating_meter_url, "%s%s", HEATING_METER_URL, customer_id);
                              printf("%s\n", heating_meter_url);

                              curl_tmp = heating_meter_url;
                              pthread_mutex_lock(&mut_curl);
                              curl_message.push(curl_tmp);
                              pthread_mutex_unlock(&mut_curl);

                              sem_post(&sem_curl);


                        }
                        balance = balance + temp_balance;
                        is_balance_needed = true;
                        /*
                         * end update mysql database
                         */

    payment_message_tag:
                        payment_message_ob =  function_payment_message(pmsg, balance, arrears, is_balance_needed, false);

                        if(payment_message_ob.client == -2 && payment_message_ob.len == -2)
                        {
                            printf("\n Exception Occured in function_payment_message()\n");
                        }
                        else
                        {
                            payment_message_ob.client = client;
                            pthread_mutex_lock(&mut_send);
                            send_queue.push(payment_message_ob);
                            pthread_mutex_unlock(&mut_send);
                            sem_post(&sem_send);
                        }

                    }
                    catch(std::exception){
                        printf("\nException in process_function().\nin payment message\n");
                    }
                    /*
                     * end of payment message
                     */
				}
				else if(memcmp((unsigned char *) test_message_type, (unsigned char *) processing_code_reversal, 6) == 0){
					/*
					 * reversal messagegg
					 */

                    try{

                            printf("In reversal message\n");
                            sprintf((char *)response_processing_code, "00");

                            //current_date time
                            char tmp_time[100];
                            time_t now = time(0);
                            now = now - (time_t)30*60;
                            tm *ltm = localtime(&now);

                            sprintf(my_year,"%d", 1900 + ltm->tm_year);
                            sprintf(my_month, "%.2d", 1 + ltm->tm_mon);
                            sprintf(my_day, "%.2d", ltm->tm_mday);

                            sprintf(my_hour, "%.2d", ltm->tm_hour);
                            sprintf(my_min, "%.2d", ltm->tm_min);
                            sprintf(my_sec, "%.2d" , ltm->tm_sec);

                            sprintf(tmp_time, "%s-%s-%s %s:%s:%s", my_year, my_month, my_day, my_hour, my_min, my_sec);

                            //field 02
                            int var_type, var_help1, var_help2;
                            for(var_type = 0; var_type < pmsg.mc.size(); var_type++)
                            {
                                if(pmsg.mc[var_type].type == 2)
                                    break;
                            }

                            if(var_type < pmsg.mc.size())
                            {
                                printf("starting creating PAN\n");
                                for(var_help2 = 0, var_help1 = 2; var_help1 < pmsg.mc[var_type].v.size(); var_help2++, var_help1++){
                                    pan[var_help2] = pmsg.mc[var_type].v[var_help1];
                                }
                                pan[var_help2] = 0;
                                printf("End of creating PAN\n");
                            }

                            //field 37
                            for(var_type; var_type < pmsg.mc.size(); var_type++){
                                if(pmsg.mc[var_type].type == 37)
                                    break;
                            }
                            if(var_type < pmsg.mc.size())
                            {
                                for(var_help1 = 0; var_help1 < pmsg.mc[var_type].v.size(); var_help1++){
                                    ref_number[var_help1] = pmsg.mc[var_type].v[var_help1];
                                }
                                ref_number[var_help1] = 0;
                            }
                            else{
                                ref_number[0] = 0;
                            }

                            //field 59
                            for(var_type; var_type < pmsg.mc.size(); var_type++){
                                if(pmsg.mc[var_type].type == 59)
                                    break;
                            }
                            if(var_type < pmsg.mc.size())
                            {
                                for(var_help1 = 0, var_help2 = 0; var_help1 < pmsg.mc[var_type].v.size() && var_help2 < 3; var_help1++){
                                    if(pmsg.mc[var_type].v[var_help1] == '|') var_help2++;
                                }
                                for( var_help1, var_help2 = 0; var_help1 < pmsg.mc[var_type].v.size(); var_help1++, var_help2++){
                                    ref_number[var_help2] = pmsg.mc[var_type].v[var_help1];
                                }
                                ref_number[var_help2] = 0;
                            }
                            else{
                                ref_number[0] = 0;
                            }


                            //customer_id & scheme_number
                            sprintf(query_string, "SELECT * FROM customers WHERE barcode='%s' ", pan);

                            printf("\n\n%s\n\n", query_string);
                            
                            wrong_response = true;
                            double tmp_balance;

                            try_for_my_sql_connection = 0;
                            while(try_for_my_sql_connection < 3){

                                if(mysql_query(con, query_string) == 0)
                                {
                                    printf("\nAfter mysql first query\n", try_for_my_sql_connection);
                                    MYSQL_RES *result = mysql_store_result(con);
                                    MYSQL_ROW row;
                                    while ((row = mysql_fetch_row(result))){
                                        sprintf(customer_id, "%s", row[0]);
                                        sprintf(scheme_number, "%s", row[25]);
                                        sprintf(meter_id, "%s", row[3]);
                                        sscanf(row[2],"%lf",&balance);
                                        sscanf(row[16] ,"%lf", &arrears);
                                    //    wrong_response = false;
                                        break;
                                    }
                                    mysql_free_result(result);
                                    break;
                                }
                                else{
                                    printf("\nReconnecting mysql database. try= %d\n", try_for_my_sql_connection);
                                    if(con != NULL){
                                        mysql_close(con);
                                    }
                                    con = mysql_init(NULL);
                                    if (con == NULL)
                                    {
                                      //  fprintf(stderr, "%s\n", mysql_error(con));
										printf("error in mysql_init()\n");
                                        //exit(1);
                                    }

                                    if (con != NULL && mysql_real_connect(con, MYSQL_IP, MYSQL_USER, MYSQL_PASS, MYSQL_DB_NAME, 0, NULL, 0) == NULL)
                                    {
                                       // fprintf(stderr, "%s\n", mysql_error(con));
                                        printf("error in database connection.");
                                       // mysql_close(con);
                                        //exit(1);
                                     }
                                    try_for_my_sql_connection++;
                                }

                            }
                            // modified in 2013/09/17
                            sprintf(query_string, "SELECT * FROM temporary_payments WHERE ref_number='%s' AND cancelled='0' AND time_date>='%s'", ref_number, tmp_time);

                            printf("\n\n%s\n\n", query_string);

                            if(mysql_query(con, query_string) == 0)
                            {
                                MYSQL_RES *result = mysql_store_result(con);
                                MYSQL_ROW row;
                                while ((row = mysql_fetch_row(result))){
                                    wrong_response = false;
                                    printf("valid data found from temporary_payments \n");
                                    break;
                                }
                                mysql_free_result(result);
                            }
                            else{
                                printf("error in mysql query\n %s\n", query_string);
                            }
                            //end modified in 2013/09/17

                            printf("In response message wrong_response = %d\n", (int)wrong_response);

                            if(wrong_response == true){
                                sprintf((char *)response_processing_code, "05");
                            }
                            else{

                                for(var_type = 0; var_type < pmsg.mc.size(); var_type++){
                                    if(pmsg.mc[var_type].type == 4)
                                        break;
                                }
                                for(var_help1 = 0; var_help1 < pmsg.mc[var_type].v.size(); var_help1++){
                                    amount[var_help1] = pmsg.mc[var_type].v[var_help1];
                                }
                                amount[var_help1] = 0;
                                sscanf(amount, "%lf", &tmp_balance);
                                tmp_balance /= 100.00;

                                //query form payment storage
                                sprintf(query_string, "SELECT * FROM payments_storage WHERE customer_id=%s ORDER BY time_date DESC", customer_id);
                                if(mysql_query(con, query_string) == 0)
                                {
                                    MYSQL_RES *result = mysql_store_result(con);
                                    MYSQL_ROW row;
                                    while ((row = mysql_fetch_row(result))){
                                        sprintf(tmp_time, "%s", row[4]);
                                        break;
                                    }
                                    mysql_free_result(result);
                                }

                                //update customer table
                                sprintf(query_string, "UPDATE customers SET balance=%lf, last_top_up='%s'  WHERE id=%s",balance - tmp_balance, tmp_time, customer_id);
                                printf("\n\n\n%s\n\n\n", query_string);
                                if (mysql_query(con, query_string))
                                  {
                                    printf("update to customers failed.\n");
                                  }

                                is_balance_needed = true;
                                balance = balance - tmp_balance;

                                //update temporary_payments
                                for(var_type = 0; var_type < pmsg.mc.size(); var_type++){
                                    if(pmsg.mc[var_type].type == 37)
                                        break;
                                }
                                if(var_type < pmsg.mc.size()){
                                    printf("ref_number =%s\n", ref_number);
                                    sprintf(query_string, "UPDATE temporary_payments SET cancelled='1'  WHERE ref_number='%s'", ref_number);
                                    printf("\n\n\n%s\n\n\n", query_string);
                                    if (mysql_query(con, query_string))
                                      {
                                        printf("update to temporary_payments failed.\n");
                                      }
                                }
                            }


                            payment_message_ob =  function_payment_message(pmsg, balance, arrears, is_balance_needed, true);

                            if(payment_message_ob.client == -2 && payment_message_ob.len == -2)
                            {
                                printf("\n Exception Occured in function_payment_message()\n");
                            }
                            else
                            {
                                printf("in server revarsal message payment_message_ob queing portion\n");
                                payment_message_ob.client = client;
                                pthread_mutex_lock(&mut_send);
                                send_queue.push(payment_message_ob);
                                pthread_mutex_unlock(&mut_send);
                                sem_post(&sem_send);
                            }
                    }
                    catch(std::exception){
                        printf("\n\nException in process_message().\nException in reversal message.\n");
                    }
                    /*
                     * end of reversal message
                     */
				}
                else{
                    printf("\n\nNothing matched\n\n");
                }
			}	
		}
		else if(memcmp((unsigned char *)pmsg.mti, (unsigned char *)void_of_payment_or_reversal, 4) == 0 ){
            try{
                cout << "in void_of_payment_or_reversal"<<endl;
                payment_message_ob =  function_void_message(pmsg);

                if(payment_message_ob.client == -2 && payment_message_ob.len == -2)
                {
                    printf("\n Exception Occured in function_payment_message()\n");
                }
                else
                {
                   int var_type, var_help1;

                    //field 37
                    for(var_type; var_type < pmsg.mc.size(); var_type++){
                        if(pmsg.mc[var_type].type == 37)
                            break;
                    }
                    if(var_type < pmsg.mc.size())
                    {
                        for(var_help1 = 0; var_help1 < pmsg.mc[var_type].v.size(); var_help1++){
                            ref_number[var_help1] = pmsg.mc[var_type].v[var_help1];
                        }
                        ref_number[var_help1] = 0;
                    }
                    else{
                        ref_number[0] = 0;
                    }


                    //customer_id & scheme_number
                    printf("ref_number =%s\n", ref_number);
                    sprintf(query_string, "UPDATE temporary_payments SET cancelled='1'  WHERE ref_number='%s'", ref_number);
                    printf("\n\n\n%s\n\n\n", query_string);

                    wrong_response = true;
                    double tmp_balance;

                    try_for_my_sql_connection = 0;
                    while(try_for_my_sql_connection < 3){

                        if(mysql_query(con, query_string) == 0)
                        {
                            printf("\nSuccessfull update of database. in void message\n", try_for_my_sql_connection);

                            break;
                        }
                        else{
                            printf("\nReconnecting mysql database. try= %d\n", try_for_my_sql_connection);
                            if(con != NULL){
                                mysql_close(con);
                            }
                            con = mysql_init(NULL);
                            if (con == NULL)
                            {
                              //  fprintf(stderr, "%s\n", mysql_error(con));
                                printf("error in mysql_init()\n");
                                //exit(1);
                            }

                            if (con != NULL && mysql_real_connect(con, MYSQL_IP, MYSQL_USER, MYSQL_PASS, MYSQL_DB_NAME, 0, NULL, 0) == NULL)
                            {
                               // fprintf(stderr, "%s\n", mysql_error(con));
                                printf("error in database connection.");
                               // mysql_close(con);
                                //exit(1);
                             }
                            try_for_my_sql_connection++;
                        }

                    }

                    payment_message_ob.client = client;

                    //for testing purpose
                    pmsg = my_msg_parcer(payment_message_ob);
                    //end for testing purpose

                    cout << "after function_void_message."<<endl;
                    pthread_mutex_lock(&mut_send);
                    send_queue.push(payment_message_ob);
                    pthread_mutex_unlock(&mut_send);
                    sem_post(&sem_send);
                }
            }
            catch(std::exception){
                printf("\n\nException in process_message().\nException in  void of payment or reversal portion.\n");
            }
		}
		/*
 		 *
 		 */ 	
	}

	mysql_close(con);
	printf("ending process_message\n");
	return arg;
}

/*
 * payment message generator function 
 */

msg_class function_payment_message(parsed_msg &pmsg, double balance, double arrears, bool is_balance_needed, bool is_reversal){
	msg_class payment_message_ob;
	
	payment_message_ob.msg = new unsigned char[500];
	payment_message_ob.len = 6;
	payment_message_ob.msg[2] = '0';
	payment_message_ob.msg[3] = '2';
	payment_message_ob.msg[4] = '1';
	payment_message_ob.msg[5] = '0';
	int j;

    try{
            /*
             * creating bit map
             */

            __u64 bm1 = 0;
            //PAN 2
            bm1 = bm1 | ((__u64)1 << (64 - 2));

            // response code 39
            bm1 = bm1 | ((__u64)1 << (64 - 39));

            // additional response data 48
            bm1 = bm1 | ((__u64)1 << (64 - 48));

            // additional amounts 54
            if( is_reversal == false){
                bm1 = bm1 | ((__u64)1 << (64 - 54));
            }

            // echo data 59
            bm1 = bm1 | ((__u64)1 << (64 - 59));

            payment_message_ob.len += 8;
            for(int i = payment_message_ob.len - 1; i >= payment_message_ob.len - 8; i--, bm1 = bm1 >> 8){
                payment_message_ob.msg[i] = (unsigned char)bm1;
            }

            /*
             * creating PAN
             */
            for(j = 0; j < pmsg.mc.size(); j++)
                if(pmsg.mc[j].type == 2)break;
            if(pmsg.mc.size() > j){
                for(int i = 0; i < pmsg.mc[j].v.size(); i++, payment_message_ob.len++){
                    payment_message_ob.msg[payment_message_ob.len] = pmsg.mc[j].v[i];
                }
            }

            /*
             * response code
             */
            for(int i = 0; i < 2; i++, payment_message_ob.len++)
                payment_message_ob.msg[payment_message_ob.len] = (unsigned char)response_processing_code[i];

            /*
             * aditional response code
             */
            unsigned char ad_res_msg[130] = "PREPAGO                 THANK YOU FOR TOPPING UPPLEASE CHECK YOUR PHONE FOR YOUR BALANCE.       ";
            __u32 ad_res_msg_len = strlen((char *)ad_res_msg);
            payment_message_ob.len += 3;

            for(int i = payment_message_ob.len -1; i >= payment_message_ob.len - 3; i--, ad_res_msg_len /= 10)
                payment_message_ob.msg[i] = (unsigned char)(ad_res_msg_len % 10) + '0';

            ad_res_msg_len = strlen((char *)ad_res_msg);

            for(int i = 0; i < ad_res_msg_len ; i++, payment_message_ob.len++)
                payment_message_ob.msg[payment_message_ob.len] = ad_res_msg[i];

            /*
             * aditional ammounts
             */
            if( is_reversal == false){
                    int amount_type = -1, currency_code = -1;
                    for(j = 0; j < pmsg.mc.size(); j++)
                        if(pmsg.mc[j].type == 4){
                            amount_type = j;
                            break;
                        }

                    for(j = 0; j < pmsg.mc.size(); j++)
                        if(pmsg.mc[j].type == 49){
                            currency_code = j;
                            break;
                        }

                    __u32 aditional_ammounts_len = 0;

                    if(amount_type != -1 && currency_code != -1 && is_balance_needed == true){
                        aditional_ammounts_len = 40;
                    }
                    payment_message_ob.len += 3;

                    for(int i = payment_message_ob.len - 1; i >= payment_message_ob.len - 3; i--, aditional_ammounts_len /= 10)
                        payment_message_ob.msg[i] = (unsigned char)(aditional_ammounts_len % 10) + '0';

                    if(amount_type != -1 && currency_code != -1 && is_balance_needed == true)
                    {
                        /*
                         *for new balance
                         */

                        unsigned char tmp[] = "0102";

                        //account type and ammount type and credit
                        for(int i = 0; i < 4; i++, payment_message_ob.len++)
                            payment_message_ob.msg[payment_message_ob.len] = tmp[i];

                        //currency code
                        for(int i = 0; i < 3; i++, payment_message_ob.len++)
                            payment_message_ob.msg[payment_message_ob.len] = pmsg.mc[currency_code].v[i];

                        //credit
                        if(balance < 0.0){
                            payment_message_ob.msg[payment_message_ob.len] = 'D';
                        }
                        else{
                            payment_message_ob.msg[payment_message_ob.len] = 'C';
                        }
                        payment_message_ob.len++;

                        //copy new balance
                        if(balance < 0.0){
                            balance = 0.0;
                        }
                        sprintf((char *)&payment_message_ob.msg[payment_message_ob.len],"%012.0lf", balance*100.00);
                        payment_message_ob.len += 12;

                        /*
                         * end new balance
                         */

                        /*
                         * for arrears balance
                         */

                        unsigned char tmp2[] = "0101";

                        //account type and ammount type and credit
                        for(int i = 0; i < 4; i++, payment_message_ob.len++)
                            payment_message_ob.msg[payment_message_ob.len] = tmp2[i];

                        //currency code
                        for(int i = 0; i < 3; i++, payment_message_ob.len++)
                            payment_message_ob.msg[payment_message_ob.len] = pmsg.mc[currency_code].v[i];

                        //credit
                        payment_message_ob.msg[payment_message_ob.len] = 'C';
                        payment_message_ob.len++;

                        //copy new balance
                        if(arrears < 0.0001){
                            arrears = 0.0;
                        }
                        else{
                            // modified accordin the specification from david all arrears = 0.00
                            arrears = 0.0;
                        }
                        sprintf((char *)&payment_message_ob.msg[payment_message_ob.len],"%012.0lf", arrears*100.00);
                        payment_message_ob.len += 12;
                        /*
                         * end for arrears balance
                         */
                    }
            }

            /*
             * echo data
             */
            for( j = 0; j < pmsg.mc.size(); j++)
                if(pmsg.mc[j].type == 59)break;

            if(pmsg.mc.size() > j){
                for(int i = 0; i < pmsg.mc[j].v.size(); i++, payment_message_ob.len++)
                    payment_message_ob.msg[payment_message_ob.len] = pmsg.mc[j].v[i];
            }

            /*
             * message_len
             */
            __u32 msg_total_len = payment_message_ob.len - 2;
            for(int i = 1; i >= 0; i--, msg_total_len = msg_total_len >> 8){
                payment_message_ob.msg[i] = (unsigned char)msg_total_len;
            }

    }
    catch(std::exception){
        printf("\nException in function_payment_message()\n");
        payment_message_ob.client = -2;
        payment_message_ob.len = -2;
    }
	return payment_message_ob;
}

msg_class function_void_message(parsed_msg &pmsg){
    msg_class payment_message_ob;
    payment_message_ob.len = 6;
    payment_message_ob.msg = new unsigned char[500];
    int j;


    try{


        strcpy((char *)&payment_message_ob.msg[2], (char *)response_to_void);

        /*
         * creating bit map
         */

        __u64 bm1 = 0;
        //PAN 2
        bm1 = bm1 | ((__u64)1 << (64 - 2));

        // response code 39
        bm1 = bm1 | ((__u64)1 << (64 - 39));


        // for echo data 59
        for( j = 0; j < pmsg.mc.size(); j++)
            if(pmsg.mc[j].type == 59){
                cout<<"we have found desired type"<<endl;
                break;
            }

        if(pmsg.mc.size() > j){
            // echo data 59
            bm1 = bm1 | ((__u64)1 << (64 - 59));
        }



        payment_message_ob.len += 8;
        for(int i = payment_message_ob.len - 1; i >= payment_message_ob.len - 8; i--, bm1 = bm1 >> 8){
            payment_message_ob.msg[i] = (unsigned char)bm1;
        }

        /*
         * creating PAN
         */
        for(j = 0; j < pmsg.mc.size(); j++)
            if(pmsg.mc[j].type == 2)break;
        if(pmsg.mc.size() > j){
            for(int i = 0; i < pmsg.mc[j].v.size(); i++, payment_message_ob.len++){
                payment_message_ob.msg[payment_message_ob.len] = pmsg.mc[j].v[i];
            }
        }

        /*
         * response code
         */
        for(int i = 0; i < 2; i++, payment_message_ob.len++)
            payment_message_ob.msg[payment_message_ob.len] = (unsigned char)'0';

        /*
         * echo data
         */
        for( j = 0; j < pmsg.mc.size(); j++)
            if(pmsg.mc[j].type == 59){
                cout<<"we have found desired type"<<endl;
                cout<<"size of echo data: "<<pmsg.mc[j].v.size()<<endl;
                break;
            }

        if(pmsg.mc.size() > j){
            for(int i = 0; i < pmsg.mc[j].v.size(); i++, payment_message_ob.len++){
                payment_message_ob.msg[payment_message_ob.len] = pmsg.mc[j].v[i];
            }
        }

        /*
         * message_len
         */
        __u32 msg_total_len = payment_message_ob.len - 2;
        for(int i = 1; i >= 0; i--, msg_total_len = msg_total_len >> 8)
            payment_message_ob.msg[i] = (unsigned char)msg_total_len;

    }
    catch(std::exception){
        printf("\nException in function function_void_message()\n");
        payment_message_ob.len = -2;
        payment_message_ob.client = -2;
    }
	return payment_message_ob;
}



/*
 * response messages are sent from this function
 */
void* send_message(void *arg){
    signal(SIGSEGV, handler);


	int client = *(int *) arg;
    msg_class tmp_message;

	int test_send;
	printf("starting send_message\n");


	while(start_sending){
		
		sem_wait(&sem_send);
		pthread_mutex_lock(&mut_send);
		tmp_message = send_queue.front();
		send_queue.pop();
		pthread_mutex_unlock(&mut_send);

		int crnt_number;
		char file_name[100];
        char cwd[200];



        //for testing purpose
        parsed_msg pmsg;
        pmsg = my_msg_parcer(tmp_message);
        //end for testing purpose

		test_send = send(tmp_message.client, tmp_message.msg, tmp_message.len, 0);
		if(test_send < 0){
			printf("error occured when sending message\n");
		}
		tmp_message.free_msg();
	}
	

	printf("ending send_message\n");
	return arg;
}

/*
 * create tcp/ip socket connection
 */

void* accept_connection(void* arg)
{   
    signal(SIGSEGV, handler);


 	int sd;
    	struct sockaddr_in addr;
   	int client;

    /*
     * creating socket
     */
    if ( (sd = socket(PF_INET, SOCK_STREAM, 0)) < 0 ){
        	PANIC("Socket");
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    /*
     * bind connection
     */
    if ( bind(sd, (struct sockaddr*)&addr, sizeof(addr)) != 0 ){
        	PANIC("Bind");
     }

    if ( listen(sd, CONN_LIMIT) != 0 ){
            PANIC("Listen");
    }
    /*
     * starting threads
     */
	start_processing = start_sending = 1;
    if ( pthread_create(&send_t, NULL, send_message, &client) != 0){
		perror("Thread creation send_message");
    }
    else {
		pthread_detach(send_t);
    }

    if( pthread_create(&process_t, NULL, process_message, &client) != 0){
		perror("Thread creation process_message");
    }
    else{
		pthread_detach(process_t);
    }



    while (start_connection)
    {
		socklen_t addr_size = sizeof(addr);
        	printf("waiting for connection\n");

        	client = accept(sd, (struct sockaddr*)&addr, &addr_size);

		start_reading = 1;

        	printf("Connected: %s:%d\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
        	
		
        if( pthread_create(&read_t, NULL, read_message, &client) != 0){
			perror("Thread creation read_message");
        }
        else{
			pthread_detach(read_t);	
        }
    }
	close(client);
	close(sd); 
	return arg;
}

/*
 * The main function
 */
int main(void)
{ 
    signal(SIGSEGV, handler);

	sem_init(&sem_receive, 0, 0);
    sem_init(&sem_send, 0, 0);
    sem_init(&sem_curl, 0, 0);
	int is_started = 0; 
	 char inp[1000];


	char cwd[500];
	char log_file_path[500];
	getcwd(cwd, sizeof(cwd));
	sprintf(log_file_path,"%s/log.txt",cwd);
	printf("%s\n", log_file_path);

	time_t now = time(0);
	tm* localtm = localtime(&now);


    //thread for curl message
    if ( pthread_create(&curl_t, NULL, process_curl, NULL) != 0 ){
                perror("Thread creation read_message");
    }
    else{
                pthread_detach(curl_t);
    }
    //end thread for curl message



	while(cin.getline(inp,999)){
		if(strcmp(inp,"start") == 0 && is_started == 0){
			start_connection = 1;
			is_started = 1;
			if ( pthread_create(&accept_t, NULL, accept_connection, NULL) != 0 )
            			perror("Thread creation read_message");
        		else
            			pthread_detach(accept_t);  /* disassociate from parent */		
		}
		else if(strcmp(inp, "stop") == 0 && is_started == 1){
			printf("stopping the server.\n");

			start_connection = 0;
			pthread_cancel (accept_t);
			printf("accept_t stopped.\n");

			start_reading = start_processing = start_sending = 0;
			pthread_cancel (read_t);
			printf("read_t stopped.\n");

			pthread_cancel (process_t);
			printf("process_T stopped.\n");

			pthread_cancel (send_t);
			printf("send_t stopped.\n");

			while(receive_queue.empty() == 0){
				msg_class tmp = receive_queue.front();
				receive_queue.pop();
				tmp.free_msg();
			}
			while(send_queue.empty() == 0){
				msg_class tmp = send_queue.front();
				send_queue.pop();
				tmp.free_msg();
			}
		}
		else if(strcmp(inp, "exit") == 0){
			if( is_started == 1){
				start_connection = 0;
				pthread_cancel (accept_t);
				start_reading = start_processing = start_sending = 0;
				pthread_cancel (read_t);
				pthread_cancel (process_t);
				pthread_cancel (send_t);
				while(receive_queue.empty() == 0){
					msg_class tmp = receive_queue.front();
					receive_queue.pop();
					tmp.free_msg();
				}
				while(send_queue.empty() == 0){
					msg_class tmp = send_queue.front();
					send_queue.pop();
					tmp.free_msg();
				}
			}
			break;
		}
		else if(inp[0] == '-' && inp[1] == 'r' && inp[2] == 'c' && inp[3] == ' '){
			response_processing_code[0] = inp[4];
			response_processing_code[1] = inp[5];
		}
	}

    string tmp_exit;
    tmp_exit = "exit";
    pthread_mutex_lock(&mut_curl);
    curl_message.push(tmp_exit);
    pthread_mutex_unlock(&mut_curl);
    sem_post(&sem_curl);
    sem_wait(&sem_exit);


    return 0;
}


