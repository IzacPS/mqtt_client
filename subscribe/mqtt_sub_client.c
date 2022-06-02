/*
 * This example shows how to write a client that subscribes to a topic and does
 * not do anything other than handle the messages that are received.
 */

#include <mosquitto.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sqlite3.h>
#include <libpq-fe.h>
#include <threads.h>

#include <cjson/cJSON.h>

#include <zlib.h>

#include "queue/sts_queue.h"
#include "timer/timer.h"
#include "log.h"

#define POSTGRESQL_PORT 5432

enum edata_type
{
    DATA_TYPE_CHAR8 = 0,
    DATA_TYPE_SHORT,
    DATA_TYPE_INT32,
    DATA_TYPE_FLOAT,
    DATA_TYPE_INT64,
    DATA_TYPE_FLOAT64,
    DATA_TYPE_STRING,
    DATA_TYPE_CUSTOM
};

#define LOCAL_DATABASE_STR "/home/pi/Documents/database/monitoramento.db"
//--------- Local SQLite database api ---------//
static sqlite3 *db;
void local_database_open()
{

	if(sqlite3_open(LOCAL_DATABASE_STR, &db) != SQLITE_OK)
	{
		LOG_ERROR("Could not open local database sqlite3!");
		sqlite3_close(db);
		exit(-1);
	}
	LOG("Local Database %s opened!\n", LOCAL_DATABASE_STR);
}

#define SQLITE_QUERY_STRING_INT "INSERT INTO %s(rede_id, dispositivo_id, valor, data_, hora_) VALUES('%s', '%s', %d, date('now'), time('now'));"
#define SQLITE_QUERY_STRING_FLOAT "INSERT INTO %s(rede_id, dispositivo_id, valor, data_, hora_) VALUES('%s', '%s', %f, date('now'), time('now'));"
#define SQLITE_QUERY_STRING_STR "INSERT INTO %s(rede_id, dispositivo_id, valor, data_, hora_) VALUES('%s', '%s', '%s', date('now'), time('now'));"
void sqlite_format_query_by_type(char *query, const char *table_name, const char *network_id, const char *device_id, void* data, int data_type)
{
	switch(data_type)
	{//TODO: Format for all the diferente sizes
		case DATA_TYPE_CHAR8:
		case DATA_TYPE_SHORT:
		case DATA_TYPE_INT32:
		{
			int in_data = *(int*)data;
			sprintf(query, SQLITE_QUERY_STRING_INT, table_name, network_id, device_id, in_data);
		}break;
		case DATA_TYPE_FLOAT:
		{
			float in_data = *(float*)data;
			sprintf(query, SQLITE_QUERY_STRING_FLOAT, table_name, network_id, device_id, in_data);
		}	
		break;
		case DATA_TYPE_INT64:
		case DATA_TYPE_FLOAT64:
		case DATA_TYPE_CUSTOM:
			break;
		case DATA_TYPE_STRING:
			sprintf(query, SQLITE_QUERY_STRING_STR, table_name, network_id, device_id, data);		
		break;
	}
}

void local_database_insert(const char *table_name, const char *network_id, const char *device_id, void *data, int data_type)
{
	char query[256];
	int query_size = 0;
	int res = 0;

	char *error_msg  = 0;
	sqlite_format_query_by_type(query, table_name, network_id, device_id, data, data_type);
	res = sqlite3_exec(db, query, 0, 0, &error_msg);

	if(res != SQLITE_OK)
	{
		LOG_ERROR("Sqlite3 : %s", error_msg);
		sqlite3_free(error_msg);
		return;
	}
}

#define local_database_close() sqlite3_close(db)


//--------- External Postgresql database api ---------//
static PGconn *conn;
int external_database_open()
{
	conn = PQconnectdb("user=postgres hostaddr=192.168.0.164 port=5432 dbname=rede_sensores password=1234567890 connect_timeout = 2");
	if(PQstatus(conn) == CONNECTION_BAD)
	{
		LOG_ERROR("Connection to external database failed: %s", PQerrorMessage(conn));
		PQfinish(conn);
		return 0;
	}
	LOG("Connected to external database!\n");
	return 1;
}

#define PG_QUERY_STRING_INT "INSERT INTO %s(rede_id, dispositivo_id, valor) VALUES('%s', '%s', %d)"
#define PG_QUERY_STRING_FLOAT "INSERT INTO %s(rede_id, dispositivo_id, valor) VALUES('%s', '%s', %f)"
#define PG_QUERY_STRING_STR "INSERT INTO %s(rede_id, dispositivo_id, valor) VALUES('%s', '%s', '%s')"
void pg_format_query_by_type(char *query, const char *table_name, const char *network_id, const char *device_id, void* data, int data_type)
{
	switch(data_type)
	{//TODO: Format for all the diferente sizes
		case DATA_TYPE_CHAR8:
		case DATA_TYPE_SHORT:
		case DATA_TYPE_INT32:
		{
			int in_data = *(int*)data;
			sprintf(query, PG_QUERY_STRING_INT, table_name, network_id, device_id, in_data);
		}break;
		case DATA_TYPE_FLOAT:
		{
			float in_data = *(float*)data;
			sprintf(query, PG_QUERY_STRING_FLOAT, table_name, network_id, device_id, in_data);
		}	
		break;
		case DATA_TYPE_INT64:
		case DATA_TYPE_FLOAT64:
		case DATA_TYPE_CUSTOM:
			break;
		case DATA_TYPE_STRING:
			sprintf(query, PG_QUERY_STRING_STR, table_name, network_id, device_id, data);		
		break;
	}
}

void external_database_insert(const char *table_name, const char *network_id, const char *device_id, void* data, int data_type)
{
	char query[256];
	PGresult *res;
	int query_size = 0; 

	pg_format_query_by_type(query, table_name, network_id, device_id, data, data_type);
	res = PQexec(conn, query);
	
	if(PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		LOG_ERROR("External database : %s", PQerrorMessage(conn));
	}
	PQclear(res);
}

#define external_database_close() PQfinish(conn)

char *table_names[] = {
	"velocidade_vento",
	"direcao_vento",
	"umidade_ar",
	"temperatura",
	"quantidade_chuva",
	"umidade_solo"
};

int sqlite_callback_is_table_empty(void *notused, int argc, char **argv, char **az_col_name)
{
	int *b = (int*)notused; 
	if(argv[0][0] == '1')
		*b = 1;
	else
		*b = 0;
	return 0;
}

int sqlite_callback_migrate_to_external(void *notused, int argc, char **argv, char **az_col_name)
{
	const char *table_name = (const char*)notused;

	PGresult *res;
	char *rede_id, *dispositivo_id, *data, *hora, *cvalor;
	float valor;
	char query[256];
	//int query_size = 0;
	int eh_direcao_vento = !strcmp(table_name, "direcao_vento");

	for(int i = 0; i < argc; i++)
	{
		if(!strcmp(az_col_name[i], "id")) 
			continue;

		if(!strcmp(az_col_name[i], "rede_id")) 
			rede_id = argv[i];
		
		if(!strcmp(az_col_name[i], "dispositivo_id")) 
			dispositivo_id = argv[i];
		
		if(!strcmp(az_col_name[i], "valor"))
		{ 
			if(eh_direcao_vento)
				cvalor = argv[i];
			else 
				sscanf(argv[i], "%f", &valor);
		}

		if(!strcmp(az_col_name[i], "data_")) 
			data = argv[i];
		
		if(!strcmp(az_col_name[i], "hora_")) 
			hora = argv[i];
		
	}


	if(eh_direcao_vento)
	{
		sprintf(query, "INSERT INTO %s(rede_id, dispositivo_id, valor, data_, hora_) VALUES('%s', '%s', '%s', '%s', '%s')",\
			table_name, rede_id, dispositivo_id, cvalor, data, hora);
		res = PQexec(conn, query);
		if(PQresultStatus(res) != PGRES_COMMAND_OK)
		{
			LOG_ERROR("External database : %s", PQerrorMessage(conn));
		}
		PQclear(res);
	}
	else
	{
		sprintf(query, "INSERT INTO %s(rede_id, dispositivo_id, valor, data_, hora_) VALUES('%s', '%s', %f, '%s', '%s')",\
			table_name, rede_id, dispositivo_id, valor, data, hora);
		res = PQexec(conn, query);
		if(PQresultStatus(res) != PGRES_COMMAND_OK)
		{
			LOG_ERROR("External database : %s", PQerrorMessage(conn));
		}
		PQclear(res);
	}
	return 0;
}

void migrate_from_local_to_external_database()
{
	char query[256];
	int res;
	char *error_msg; 
	int has_value = 0;

	local_database_open();
	if(!external_database_open())
		return;

	for(int i = 0; i < 6; i++)
	{
		sprintf(query, "SELECT count(*) FROM (select 0 from %s limit 1);", table_names[i]);
		res = sqlite3_exec(db, query, sqlite_callback_is_table_empty, &has_value, &error_msg);
		
		if(res != SQLITE_OK)
		{
			LOG_ERROR("Sqlite3 : %s", error_msg);
			sqlite3_free(error_msg);
		}		

		if(has_value)
		{
			sprintf(query, "SELECT * FROM %s;", table_names[i]);
			res = sqlite3_exec(db, query, sqlite_callback_migrate_to_external, table_names[i], &error_msg);

			if(res != SQLITE_OK)
			{
				LOG_ERROR("Sqlite3 : %s", error_msg);
				sqlite3_free(error_msg);
			}
		}
		sprintf(query, "DELETE FROM %s;", table_names[i]);
		res = sqlite3_exec(db, query, 0, 0, &error_msg);
		if(res != SQLITE_OK)
		{
			LOG_ERROR("Sqlite3 : %s", error_msg);
			sqlite3_free(error_msg);
		}
	}

	external_database_close();
	local_database_close();
}

int sqlite_callback_get_data(void *notused, int argc, char **argv, char **az_col_name)
{
	cJSON *arr = (cJSON *)notused;

	PGresult *res;
	char *data, *hora, *cvalor;
	float valor;

	cJSON *obj = cJSON_CreateObject();
	cJSON_AddItemToArray(arr, obj);
	cJSON *field = NULL;

	for(int i = 0; i < argc; i++)
	{		
		if(!strcmp(az_col_name[i], "valor"))
		{ 
				//cvalor = argv[i];
			field = cJSON_CreateString(argv[i]);
			cJSON_AddItemToObject(obj, "v", field);
		}

		if(!strcmp(az_col_name[i], "data_"))
		{
			//data = argv[i];
			field = cJSON_CreateString(argv[i]);
			cJSON_AddItemToObject(obj, "d", field);
		}

		if(!strcmp(az_col_name[i], "hora_"))
		{ 
			//hora = argv[i];
			field = cJSON_CreateString(argv[i]);
			cJSON_AddItemToObject(obj, "h", field);
		}
	}

	return 0;
}

struct received_data_t
{
	struct mosquitto * mosquitto;
	void *obj;
	struct mosquitto_message *message;
};

static StsHeader *queue;
static StsHeader *response_queue;

// int run_database_query(void *data)
// {
// 	if (!data)
// 		return 0;

// 	struct received_data_t *recv_data = (struct received_data_t *)(data);

// 	const cJSON * field = NULL;

// 	cJSON *json_data = cJSON_ParseWithLength(recv_data->message->payload, recv_data->message->payloadlen);

// 	if(!json_data)
// 	{
// 		const char * error_ptr = cJSON_GetErrorPtr();
// 		if(error_ptr)
// 		{
// 			printf("Error before: %s\n", error_ptr);
// 		}
// 		return 0;
// 	}

// 	const char *type;
// 	const char *table_name;
// 	const char *data_min;
// 	const char *data_max;

// 	field = cJSON_GetObjectItemCaseSensitive(json_data, "type");
//     if (cJSON_IsString(field) && (field->valuestring != NULL))
//     {
//         type = field->valuestring;
//     }

// 	field = cJSON_GetObjectItemCaseSensitive(json_data, "table");
//     if (cJSON_IsString(field) && (field->valuestring != NULL))
//     {
//         table_name = field->valuestring;
//     }

// 	field = cJSON_GetObjectItemCaseSensitive(json_data, "data_min");
//     if (cJSON_IsString(field) && (field->valuestring != NULL))
//     {
//         data_min = field->valuestring;
//     }

// 	field = cJSON_GetObjectItemCaseSensitive(json_data, "data_max");
//     if (cJSON_IsString(field) && (field->valuestring != NULL))
//     {
//         data_max = field->valuestring;
//     }


// 	char query[256];

// 	if(!strcmp(type, "unique"))
// 	{
// 		sprintf(
// 			query, 
// 			"SELECT valor,data_,hora_ FROM %s WHERE data_ = '%s';", 
// 			table_name, data_min);
// 	}

// //		sprintf(
// //			query, 
// //			"SELECT valor,data_,hora_ FROM %s WHERE data_ BETWEEN '%s' AND '%s';", 
// //			table_name, data_min, data_max);


// 	cJSON_Delete(json_data);

// 	int res;
// 	char *error_msg;

// 	char *str = NULL;
// 	json_data = cJSON_CreateObject();
// 	cJSON *arr = cJSON_CreateArray();
// 	cJSON_AddItemToObject(json_data, "arr", arr);

// 	local_database_open();

// 	res = sqlite3_exec(db, query, sqlite_callback_get_data, arr, &error_msg);
	
// 	if(res != SQLITE_OK)
// 	{
// 		LOG_ERROR("Sqlite3 : %s", error_msg);
// 		sqlite3_free(error_msg);
// 	}		

// 	local_database_close();

// 	str = cJSON_PrintUnformatted(json_data);

// 	uLong size = strlen(str);
// 	uLong compressed_size = size + 4;
// 	char *compressed_data = calloc(compressed_size, 1);
	
// 	int * original_data_size = (int *)(compressed_data);

// 	//size + 5 where the first 5 bytes are the original size of the file
// 	int err = compress(compressed_data + 4, &compressed_size, str, size);

// 	if(err == Z_OK)
// 	{
// 		(*(int *)(original_data_size)) = size + 256;

// 		mosquitto_publish(
// 			recv_data->mosquitto, 
// 			NULL, 
// 			"net0/database/recv",
// 			compressed_size + 4,
// 			compressed_data,
// 			1, false);
// 	}

	
// 	cJSON_Delete(json_data);

// 	free(recv_data->message->topic);
// 	free(recv_data->message->payload);
// 	free(recv_data->message);
// 	free(recv_data);
// 	free(compressed_data);

// 	return 0;
// }

int response_run(void *_data)
{
	while(true)
	{
		if(!StsQueue.is_empty(response_queue))
		{
			struct received_data_t *recv_data = StsQueue.pop(response_queue);
			if(!recv_data)
				continue;

			printf("message posted\n");
			const cJSON * field = NULL;

			cJSON *json_data = cJSON_ParseWithLength(recv_data->message->payload, recv_data->message->payloadlen);

			if(!json_data)
			{
				const char * error_ptr = cJSON_GetErrorPtr();
				if(error_ptr)
				{
					printf("Error before: %s\n", error_ptr);
				}
				return 0;
			}

			const char *type;
			const char *table_name;
			const char *data_min;
			const char *data_max;
			const char *compression;

			field = cJSON_GetObjectItemCaseSensitive(json_data, "type");
			if (cJSON_IsString(field) && (field->valuestring != NULL))
			{
				type = field->valuestring;
			}

			field = cJSON_GetObjectItemCaseSensitive(json_data, "table");
			if (cJSON_IsString(field) && (field->valuestring != NULL))
			{
				table_name = field->valuestring;
			}

			field = cJSON_GetObjectItemCaseSensitive(json_data, "data_min");
			if (cJSON_IsString(field) && (field->valuestring != NULL))
			{
				data_min = field->valuestring;
			}

			field = cJSON_GetObjectItemCaseSensitive(json_data, "data_max");
			if (cJSON_IsString(field) && (field->valuestring != NULL))
			{
				data_max = field->valuestring;
			}

			field = cJSON_GetObjectItemCaseSensitive(json_data, "compression");
			if (cJSON_IsString(field) && (field->valuestring != NULL))
			{
				compression = field->valuestring;
			}


			char query[256];

			if(!strcmp(type, "unique"))
			{
				sprintf(
					query, 
					"SELECT valor,data_,hora_ FROM %s WHERE data_ = '%s';", 
					table_name, data_min);
			}

			if(!strcmp(type, "range"))
			{
				sprintf(
					query, 
					"SELECT valor,data_,hora_ FROM %s WHERE data_ BETWEEN '%s' AND '%s';", 
					table_name, data_min, data_max);
			}




			int res;
			char *error_msg;

			char *str = NULL;
			cJSON *json = cJSON_CreateObject();
			cJSON *arr = cJSON_CreateArray();
			cJSON_AddItemToObject(json, "arr", arr);

			local_database_open();

			res = sqlite3_exec(db, query, sqlite_callback_get_data, arr, &error_msg);
			
			if(res != SQLITE_OK)
			{
				LOG_ERROR("Sqlite3 : %s", error_msg);
				sqlite3_free(error_msg);
			}		

			local_database_close();

			str = cJSON_PrintUnformatted(json);

			if(!strcmp(compression, "enable"))
			{
				uLong size = strlen(str);
				uLong compressed_size = size + 4 + 1;
				char *compressed_data = calloc(compressed_size, 1);
				
				char* compressed_flag = (char*)(compressed_data);
				int * original_data_size = (int *)(compressed_data + 1);

				int err = compress(compressed_data + 4 + 1, &compressed_size, str, size);

				if(err == Z_OK)
				{
					*(char*)(compressed_flag) = 1;
					*(int *)(original_data_size) = size + 256;

					mosquitto_publish(
						recv_data->mosquitto, 
						NULL, 
						"net0/database/recv",
						compressed_size + 4 + 1,
						compressed_data,
						1, false);
				}
				free(compressed_data);	
			}
			else
			{
				int str_len = strlen(str);

				char * data = calloc(str_len + 1, 1);
				
				memcpy(data + 1, str, str_len);

				//compressed flag
				*(char*)data = 0;

				mosquitto_publish(
					recv_data->mosquitto, 
					NULL, 
					"net0/database/recv",
					str_len + 1,
					data,
					1, false);

				free(data);
			}

			cJSON_Delete(json_data);
			cJSON_Delete(json);

			free(recv_data->message->topic);
			free(recv_data->message->payload);
			free(recv_data->message);
			free(recv_data);

		}
	}
	return 0;
}



//----- Send to Database api -----//
struct sensor_net_database_info
{
	char network_id[5];
	char device_id[5];
	char database_table_name[32];
};

void get_sensor_net_database_info(const char *topic, struct sensor_net_database_info *info)
{
	memset(info, 0, sizeof(struct sensor_net_database_info));
	memcpy(info->network_id, topic, 4);
	memcpy(info->device_id, topic + 5, 4);
	int size = strlen(topic) - 10;
	memcpy(info->database_table_name, topic + 10, size);
}


int send_to_database(void *data)
{
	struct timer_t time;
	timer_init(&time);

	timer_start(&time, 86400000);
	while(true)
	{
		if(timer_is_time_up(&time))
		{
			LOG("Migrating from local to external database.\n");
			migrate_from_local_to_external_database();
			timer_start(&time, 86400000);
		}
		if(!StsQueue.is_empty(queue))
		{
			struct mosquitto_message *msg = StsQueue.pop(queue);
			if(!msg)
				continue;
			char *payload = (char*)msg->payload;
			char data_type = *payload++;
			char data_size = *payload++;
			
			struct sensor_net_database_info info;
			get_sensor_net_database_info(msg->topic, &info);

			if(external_database_open())
			{
				external_database_insert(info.database_table_name
				, info.network_id, info.device_id, payload, data_type);
				external_database_close();
			}
			else
			{
				local_database_open();
				local_database_insert(info.database_table_name, info.network_id, info.device_id, payload, data_type);
				local_database_close();
			}
			free(msg->topic);
			free(msg->payload);
			free(msg);
		}
	}
	return 0;
}


//---------------  MQTT API  ------------------//
/* Callback called when the client receives a CONNACK message from the broker. */
void on_connect(struct mosquitto *mosq, void *obj, int reason_code)
{
	int rc;
	/* Print out the connection result. mosquitto_connack_string() produces an
	 * appropriate string for MQTT v3.x clients, the equivalent for MQTT v5.0
	 * clients is mosquitto_reason_string().
	 */

	LOG("on_connect: \n", mosquitto_connack_string(reason_code));
	if(reason_code != 0){
		/* If the connection fails for any reason, we don't want to keep on
		 * retrying in this example, so disconnect. Without this, the client
		 * will attempt to reconnect. */
		mosquitto_disconnect(mosq);
	}

	/* Making subscriptions in the on_connect() callback means that if the
	 * connection drops and is automatically resumed by the client, then the
	 * subscriptions will be recreated when the client reconnects. */
	rc = mosquitto_subscribe(mosq, NULL, "net0/database", 1);
	if(rc != MOSQ_ERR_SUCCESS){
		LOG_ERROR("Error subscribing: %s", mosquitto_strerror(rc));	
		mosquitto_disconnect(mosq);
	}
	rc = mosquitto_subscribe(mosq, NULL, "net0/sta0/velocidade_vento", 1);
	if(rc != MOSQ_ERR_SUCCESS){
		LOG_ERROR("Error subscribing: %s", mosquitto_strerror(rc));
		/* We might as well disconnect if we were unable to subscribe */
		mosquitto_disconnect(mosq);
	}
	rc = mosquitto_subscribe(mosq, NULL, "net0/sta0/direcao_vento", 1);
	if(rc != MOSQ_ERR_SUCCESS){
		LOG_ERROR("Error subscribing: %s", mosquitto_strerror(rc));
		/* We might as well disconnect if we were unable to subscribe */
		mosquitto_disconnect(mosq);
	}
    //{"sta1/wind/rpm", inform_wind_rpm},
    rc = mosquitto_subscribe(mosq, NULL, "net0/sta0/umidade_ar", 1);
	if(rc != MOSQ_ERR_SUCCESS){
		LOG_ERROR("Error subscribing: %s", mosquitto_strerror(rc));
		/* We might as well disconnect if we were unable to subscribe */
		mosquitto_disconnect(mosq);
	}
    rc = mosquitto_subscribe(mosq, NULL, "net0/sta0/temperatura", 1);
	if(rc != MOSQ_ERR_SUCCESS){
		LOG_ERROR("Error subscribing: %s", mosquitto_strerror(rc));
		/* We might as well disconnect if we were unable to subscribe */
		mosquitto_disconnect(mosq);
	}
    rc = mosquitto_subscribe(mosq, NULL, "net0/sta0/quantidade_chuva", 1);
	if(rc != MOSQ_ERR_SUCCESS){
		LOG_ERROR("Error subscribing: %s", mosquitto_strerror(rc));
		/* We might as well disconnect if we were unable to subscribe */
		mosquitto_disconnect(mosq);
	}
}

void on_publish(struct mosquitto *mosq, void *obj, int mid)
{

}

/* Callback called when the broker sends a SUBACK in response to a SUBSCRIBE. */
void on_subscribe(struct mosquitto *mosq, void *obj, int mid, int qos_count, const int *granted_qos)
{
	int i;
	bool have_subscription = false;

	/* In this example we only subscribe to a single topic at once, but a
	 * SUBSCRIBE can contain many topics at once, so this is one way to check
	 * them all. */
	for(i=0; i<qos_count; i++){
		LOG("on_subscribe: %d - granted qos = %d\n", i, granted_qos[i]);
		if(granted_qos[i] <= 2){
			have_subscription = true;
		}
	}
	if(have_subscription == false){
		/* The broker rejected all of our subscriptions, we know we only sent
		 * the one SUBSCRIBE, so there is no point remaining connected. */
		LOG_ERROR("Error: All subscriptions rejected.");
		mosquitto_disconnect(mosq);
	}
}


/* Callback called when the client receives a message. */
void on_message(struct mosquitto *mosq, void *obj, const struct mosquitto_message *msg)
{
	struct mosquitto_message *in_msg = malloc(sizeof(struct mosquitto_message));
	in_msg->topic = strdup(msg->topic);
	in_msg->payload = malloc(msg->payloadlen);
	memcpy(in_msg->payload, msg->payload, msg->payloadlen);
	in_msg->mid = msg->mid;
	in_msg->qos = msg->qos;
	in_msg->retain = msg->retain;
	in_msg->payloadlen = msg->payloadlen;

	if(!strcmp(msg->topic, "net0/database"))
	{
		struct received_data_t *received_data = malloc(sizeof(struct received_data_t));
		received_data->message = in_msg;
		received_data->obj = obj;
		received_data->mosquitto = mosq;

		StsQueue.push(response_queue, received_data);
		//thrd_t thread_handle;
		//thrd_create(&thread_handle, run_database_query, received_data);
		return;
	}


	StsQueue.push(queue, in_msg);
}


int main(int argc, char *argv[])
{
	struct mosquitto *mosq;
	int rc;
	thrd_t thread_handle;
	thrd_t thread_handle_response;

	queue = StsQueue.create();
	response_queue = StsQueue.create();

	thrd_create(&thread_handle, send_to_database, 0);
	thrd_create(&thread_handle_response, response_run, 0);

	/* Required before calling other mosquitto functions */
	mosquitto_lib_init();
	/* Create a new client instance.
	 * id = NULL -> ask the broker to generate a client id for us
	 * clean session = true -> the broker should remove old sessions when we connect
	 * obj = NULL -> we aren't passing any of our private data for callbacks
	 */
	mosq = mosquitto_new(NULL, true, NULL);
	if(mosq == NULL){
		LOG_ERROR("Error: Out of memory.");
		return 1;
	}

	/* Configure callbacks. This should be done before connecting ideally. */
	mosquitto_connect_callback_set(mosq, on_connect);
	mosquitto_subscribe_callback_set(mosq, on_subscribe);
	mosquitto_message_callback_set(mosq, on_message);
	mosquitto_publish_callback_set(mosq, on_publish);
	//mosquitto_disconnect_callback_set(mosq, on_disconnect);

	/* Connect to test.mosquitto.org on port 1883, with a keepalive of 60 seconds.
	 * This call makes the socket connection only, it does not complete the MQTT
	 * CONNECT/CONNACK flow, you should use mosquitto_loop_start() or
	 * mosquitto_loop_forever() for processing net traffic. */
	rc = mosquitto_connect(mosq, "broker.emqx.io", 1883, 60);
	if(rc != MOSQ_ERR_SUCCESS){
		mosquitto_destroy(mosq);
		LOG_ERROR("%s\n", mosquitto_strerror(rc));
		return 1;
	}

	/* Run the network loop in a blocking call. The only thing we do in this
	 * example is to print incoming messages, so a blocking call here is fine.
	 *
	 * This call will continue forever, carrying automatic reconnections if
	 * necessary, until the user calls mosquitto_disconnect().
	 */
	mosquitto_loop_forever(mosq, -1, 1);

	mosquitto_lib_cleanup();

	StsQueue.destroy(queue);
	StsQueue.destroy(response_queue);
	return 0;
}

