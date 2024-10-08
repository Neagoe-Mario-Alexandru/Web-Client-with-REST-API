#include <stdio.h>
#include <fstream>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <bits/stdc++.h>
#include "json.hpp"
#include "helpers.h"
#include "requests.h"

namespace jason = nlohmann;
using namespace std;

#define S_PORT "8080"
#define S_IP "34.246.184.49"

char *server_ip = S_IP;
char *server_port = S_PORT;

char cookie[1024] = {0};
char token_jwt[1024] = {0};

// Campuri care vor fi completate de autilizator pentru
// a gestiona o carte
const char to_complete_book[][20] = {
	"title",
	"author",
	"genre",
	"publisher",
	"page_count",
};

// Campuri care vor fi completate de autilizator pentru
// autentificare
const char to_complete_autenthification[][20] = {
	"username",
	"password",
};

// Pentru id carti
const char to_complete_id[][20] = {
	"id",
};

// Afisez utilizatorului ce trebuie sa completeze si apoi
// salvez datele introduse de utilizator intr-un obiect json
// Aici mai si verific datele pentru add_books
int check_prompts(const char to_complete[][20], int nr, jason::ordered_json &data)
{
	char buf[1024];
	size_t len_buf;
	int i;

	for (i = 0; i < nr; i++)
	{
		cout << to_complete[i] << "=";
		if (fgets(buf, sizeof(buf), stdin) == NULL)
		{
			return -1;
		}

		len_buf = strlen(buf) - 1;
		if (len_buf > 0 && buf[len_buf] == '\n')
		{
			buf[len_buf] = '\0';
		}

		// Verific sa nu am input gol, in general nu pentru add_books
		if (strlen(buf) == 0)
		{
			return -1;
		}

		// Verific page count si il fac int
		// Verific si celelalte campuri
		if (strcmp(to_complete[i], "page_count") == 0)
		{
			for (char *b = buf; *b; b++)
			{
				if (*b < '0' || *b > '9')
				{
					return -1;
				}
			}
			data[to_complete[i]] = atoi(buf);
		}
		else if (strcmp(to_complete[i], "author") == 0 ||
				 strcmp(to_complete[i], "genre") == 0 ||
				 strcmp(to_complete[i], "title") == 0 ||
				 strcmp(to_complete[i], "publisher") == 0)
		{
			// Verific ca nu cumva sa nu primesc camp gol
			if (strlen(buf) == 0)
			{
				return -1;
			}
			data[to_complete[i]] = buf;
		}
		else
		{
			data[to_complete[i]] = buf;
		}
	}
	return 0;
}

// Construiesc mesajul HTTP
char *http_header(char *method, char *path, jason::ordered_json &header, char *content)
{

	char *message = (char *)calloc(BUFLEN, sizeof(char));
	// Ca sa iau linie cu linie
	char *header_l = (char *)calloc(LINELEN, sizeof(char));

	// Prima linie format HTTP
	// method = "GET", "POST", etc.
	sprintf(header_l, "%s %s HTTP/1.1", method, path);
	compute_message(message, header_l);

	header_l[0] = 0;

	// Adaug linie pentru fiecare obiect json din header
	for (const auto &each : header.items())
	{
		strcat(header_l, each.key().c_str());
		strcat(header_l, ": ");
		if (each.value().is_string())
		{
			string str = each.value();
			strcat(header_l, str.c_str());
		}
		compute_message(message, header_l);
		header_l[0] = 0;
	}
	compute_message(message, "");

	// Daca nu am continut
	if (content == NULL)
	{
		free(header_l);
		return message;
	}
	strcat(message, content);
	free(header_l);
	return message;
}

// Aici fac si gestionarea codului primit,
// daca e eroare sau nu
int check_response(char *response, int op)
{
	// Ex: Orice cu 200 e bun
	// Orice care e cu 300, 400, 500 e o problema
	int error_code;
	sscanf(response, "HTTP/1.1 %d", &error_code);
	// Daca am un cod care sa indice o eroare
	if (error_code >= 200 && error_code <= 299)
	{
		return 0;
	}

	// Verific sa vad daca am un mesaj valid de la server
	char *message = basic_extract_json_response(response);
	if (message == NULL)
	{
		return 2;
	}
	// Ca sa extrag eroarea, daca am una
	auto error = jason::json::parse(message);
	const string &err = error.at("error");
	cout << "Error: " << err.c_str();
	cout << '\n';
	return 1;
}

void logout()
{
	// Nu pot sa ma deloghez daca nu sunt logat
	if (strlen(cookie) == 0)
	{
		cout << "Error: You are not logged in!\n";
		return;
	}
	// Construiesc header
	jason::ordered_json http;
	http["Host"] = S_IP ":" S_PORT;
	http["Cookie"] = cookie;
	char *message = http_header("GET", "/api/v1/tema/auth/logout",
								http, NULL);

	// Deschid conexiunea si trimit la server
	int connection = open_connection(S_IP, atoi(S_PORT), AF_INET, SOCK_STREAM, 0);

	send_to_server(connection, message);
	char *response = receive_from_server(connection);
	// 7 e indexul lui logout
	// Asa am facut la fiecare functie, i-am dat indexul
	check_response(response, 7);
	close_connection(connection);
	cout << "Succes: User logged out!\n";
	// Sterg cookie si token-ul si eliberez memoria
	memset(token_jwt, 0, sizeof(token_jwt));
	memset(cookie, 0, sizeof(cookie));
	free(message);
	free(response);
}

// Ca sa verific in login si register sa nu am spatii
bool has_no_spaces(const string &str)
{
	return str.find(' ') == string::npos;
}

// Doar functia in sine e 'registration', comanda primita tot 'register' este
void registration()
{
	// Verific sa nu ma pot inregistra cand sunt logat
	jason::ordered_json from_keyboard;
	if (strlen(cookie) != 0 || strlen(token_jwt) != 0)
	{
		cout << "Error: You can't register while being logged in!\n";
		return;
	}
	check_prompts(to_complete_autenthification, 2, from_keyboard);

	// Construiesc acel header
	jason::ordered_json header;
	header["Content-Type"] = "application/json";
	// Si verific sa nu primesc spatii
	string dumper = from_keyboard.dump();
	if (has_no_spaces(dumper) == false)
	{
		cout << "Error: You can't use space character. Use 'register' again!\n";
		return;
	}
	char *ptr = (char *)dumper.c_str();
	header["Content-Length"] = to_string(strlen(ptr));
	header["Host"] = S_IP ":" S_PORT;
	char *message = http_header("POST", "/api/v1/tema/auth/register", header, ptr);

	int connection = open_connection(S_IP, atoi(S_PORT), AF_INET, SOCK_STREAM, 0);
	send_to_server(connection, message);
	char *response = receive_from_server(connection);
	//  0 pentru register
	int printing;
	printing = check_response(response, 0);
	if (printing == 0)
	{
		cout << "Succes: User registered!\n";
	}
	close_connection(connection);
	free(message);
	free(response);
}

void login()
{
	// Nu pot sa dau login cand sunt deja logat
	if (strlen(cookie) != 0 || strlen(token_jwt) != 0)
	{
		cout << "Error: You need to log out first!\n";
		return;
	}

	// La fel ca la register, verific sa nu am spatii
	jason::ordered_json from_keyboard;
	check_prompts(to_complete_autenthification, 2, from_keyboard);
	string dumper = from_keyboard.dump();
	if (has_no_spaces(dumper) == false)
	{
		cout << "Error: You can't use space character. Use 'login' again!\n";
		return;
	}

	int connection = open_connection(server_ip, atoi(server_port), AF_INET, SOCK_STREAM, 0);
	const char *to_char = dumper.c_str();
	char combined[20];
	snprintf(combined, sizeof(combined), "%s:%s", S_IP, S_PORT);
	char *message = compute_post_request(combined,
										 "/api/v1/tema/auth/login", "application/json",
										 (char **)&to_char, 1, NULL, 0);
	send_to_server(connection, message);
	free(message);
	char *response = receive_from_server(connection);
	int printing = check_response(response, 1);
	// Daca am esuat
	if (printing)
	{
		free(response);
		return;
	}
	// Daca e bine
	if (printing == 0)
	{
		cout << "Succes: User logged in succesfully!\n";
	}

	// Extrag acel cookie din raspuns si il setez
	char *cookie_begin = strstr(response, "Set-Cookie: ");
	char *cookie_end = strchr(cookie_begin, ';');
	cookie_begin = cookie_begin + strlen("Set-Cookie: ");
	memcpy(cookie, cookie_begin, cookie_end - cookie_begin);
	close_connection(connection);
	free(response);
}

void enter_library()
{
	// Nu pot daca nu sunt logat
	if (strlen(cookie) == 0)
	{
		cout << "Eroare: You are not logged in!\n";
	}
	// Ca sa am S_IP si S_PORT impreuna
	char combined[20];
	snprintf(combined, sizeof(combined), "%s:%s", S_IP, S_PORT);
	// Probleme daca dadeam direct cookie, asa ca am facut o copie
	char *cookie_holder = cookie;
	char *message = compute_get_request(combined,
										"/api/v1/tema/library/access", NULL,
										(char **)&cookie_holder, 1);
	int connection = open_connection(S_IP, atoi(S_PORT), AF_INET, SOCK_STREAM, 0);
	send_to_server(connection, message);
	char *response = receive_from_server(connection);
	int printing;
	printing = check_response(response, 2);
	// Daca e okay
	if (printing == 0)
	{
		// Din ce primesc de la server, parsez raspunsul
		// si extrag tokenul jwt
		cout << "Succes: User has access to library!\n";
		char *payload_begin = basic_extract_json_response(response);
		jason::ordered_json tempo = jason::ordered_json::parse(payload_begin);
		const string &t = tempo.at("token");
		memcpy(token_jwt, t.c_str(), t.length());
		close_connection(connection);
		free(message);
		free(response);
	}
	else
	{
		free(response);
		return;
	}
}

void get_books()
{
	// TREBUIE ENTER LIBRARY, NU UITA
	// Daca nu, nu pot sa accesez
	if (strlen(token_jwt) == 0)
	{
		cout << "Error: You didn't enter library!\n";
		return;
	}
	jason::ordered_json header;
	header["Authorization"] = "Bearer " + string(token_jwt);
	header["Cookie"] = cookie;
	header["Host"] = S_IP ":" S_PORT;
	char *message = http_header("GET", "/api/v1/tema/library/books",
								header, NULL);
	int connection = open_connection(server_ip, atoi(server_port), AF_INET, SOCK_STREAM, 0);
	send_to_server(connection, message);
	char *response = receive_from_server(connection);
	int printing;
	printing = check_response(response, 3);
	char *books = strstr(response, "[");
	// Daca primesc raspuns pozitiv de la server, afisez cartile
	// Si din cum am vazut ca arata afisarea JSON, si cand nu am
	// carti si cand am, apare [ si apoi cartile
	if (printing == 0)
	{
		// TODO: SA VAD CUM FAC AFISAREA AICI
		cout << books << '\n';
	}
	close_connection(connection);
	free(message);
	free(response);
}

void get_book()
{
	if (strlen(token_jwt) == 0)
	{
		cout << "Error: You didn't enter library!\n";
		return;
	}
	// Extrag ID
	jason::ordered_json dumper;
	check_prompts(to_complete_id, 1, dumper);
	string keyb = dumper.dump();
	char from_keyboard[50];
	strcpy(from_keyboard, keyb.c_str());
	// Am calculat exact de unde trebuie sa extrag acel id
	strcpy(from_keyboard, from_keyboard + 7);
	int len_key = strlen(from_keyboard) - 2;
	from_keyboard[len_key] = '\0';

	jason::ordered_json header;
	header["Authorization"] = "Bearer " + string(token_jwt);
	header["Cookie"] = cookie;
	header["Host"] = S_IP ":" S_PORT;
	const char *base_path = "/api/v1/tema/library/books/";
	// Construiesc path prin unirea base_path cu id
	char path[100];
	strcpy(path, base_path);
	strcat(path, from_keyboard);

	char *message = http_header("GET", path, header, NULL);
	int connection = open_connection(server_ip, atoi(server_port), AF_INET, SOCK_STREAM, 0);
	send_to_server(connection, message);
	char *response = receive_from_server(connection);
	int printing;
	printing = check_response(response, 4);

	char *book = basic_extract_json_response(response);
	if (printing != 0)
	{
		close_connection(connection);
		free(message);
		free(response);
		return;
	}
	if (book == NULL)
	{
		free(response);
		return;
	}
	// Afisez frumos cartea folosind dump
	auto resp = jason::ordered_json::parse(book);
	std::cout << resp.dump(4) << "\n";
	free(response);
	close_connection(connection);
}

void add_book()
{
	if (strlen(token_jwt) == 0)
	{
		cout << "Error: You didn't enter the library!\n";
		return;
	}

	jason::ordered_json from_keyboard;
	// Verific sa nu am input invalid, aici am avut probleme
	if (check_prompts(to_complete_book, 5, from_keyboard) == -1)
	{
		cout << "Error: Invalid input. Book not added.\n";
		return;
	}

	string dumper = from_keyboard.dump();
	char *content = (char *)dumper.c_str();

	// Construiesc header
	jason::ordered_json header;
	header["Authorization"] = "Bearer " + string(token_jwt);
	header["Cookie"] = cookie;
	header["Host"] = S_IP ":" S_PORT;
	header["Content-Type"] = "application/json";
	header["Content-Length"] = to_string(strlen(content));

	char *message = http_header("POST", "/api/v1/tema/library/books", header, content);

	int connection = open_connection(server_ip, atoi(server_port), AF_INET, SOCK_STREAM, 0);

	send_to_server(connection, message);

	char *response = receive_from_server(connection);
	int printing = check_response(response, 5);
	if (printing == 0)
	{
		cout << "Success: Book added!\n";
	}
	close_connection(connection);
	free(message);
	free(response);
}

void delete_book()
{
	if (strlen(token_jwt) == 0)
	{
		cout << "Error: You didn't enter library!\n";
		return;
	}
	jason::ordered_json dumper;
	check_prompts(to_complete_id, 1, dumper);
	string keyb = dumper.dump();
	char from_keyboard[50];
	strcpy(from_keyboard, keyb.c_str());
	// La fel ca la get book, calculez unde am id
	// Functioneaza cam pe acelasi principiu cu get_book,
	// tot dupa id ma uit
	// Inclusiv cum formez path
	strcpy(from_keyboard, from_keyboard + 7);
	int len_key = strlen(from_keyboard) - 2;
	from_keyboard[len_key] = '\0';
	len_key = len_key + 1;

	jason::ordered_json header;
	header["Authorization"] = "Bearer " + string(token_jwt);
	header["Cookie"] = cookie;
	header["Host"] = S_IP ":" S_PORT;
	const char *base_path = "/api/v1/tema/library/books/";
	char path[100];
	strcpy(path, base_path);
	strcat(path, from_keyboard);
	char *message = http_header("DELETE", path, header, NULL);
	int connection = open_connection(server_ip, atoi(server_port), AF_INET, SOCK_STREAM, 0);
	send_to_server(connection, message);
	char *response = receive_from_server(connection);
	int printing;
	printing = check_response(response, 6);
	if (printing == 0)
	{
		cout << "Succes: User deleted the book!\n";
	}
	close_connection(connection);
	free(message);
	free(response);
}

int main(int argc, char *argv[])
{
	char tastatura[1024];
	int okay, i;
	while (1)
	{
		okay = 0;
		fgets(tastatura, sizeof(tastatura), stdin);
		int tastatura_len = strlen(tastatura);
		if (tastatura_len > 0 && tastatura[tastatura_len - 1] == '\n')
		{
			tastatura[tastatura_len - 1] = '\0';
		}
		// Iau fiecare comanda pe rand si apelez functia pentru ea
		// mai putin exit, acolo doar dau return si nu trebuie
		// sa afisez ceva
		if (strcmp(tastatura, "exit") == 0)
		{
			return 0;
		}
		if (strcmp(tastatura, "login") == 0)
		{
			okay = 1;
			login();
		}
		if (strcmp(tastatura, "register") == 0)
		{
			okay = 1;
			registration();
		}
		if (strcmp(tastatura, "enter_library") == 0)
		{
			okay = 1;
			enter_library();
		}
		if (strcmp(tastatura, "get_books") == 0)
		{
			okay = 1;
			get_books();
		}
		if (strcmp(tastatura, "get_book") == 0)
		{
			okay = 1;
			get_book();
		}
		if (strcmp(tastatura, "add_book") == 0)
		{
			okay = 1;
			add_book();
		}
		if (strcmp(tastatura, "delete_book") == 0)
		{
			okay = 1;
			delete_book();
		}
		if (strcmp(tastatura, "logout") == 0)
		{
			okay = 1;
			logout();
		}
		// Daca nu e nicio comanda valida
		if (okay == 0)
		{
			cout << "Error: Invalid command, please introduce";
			cout << " another comand!\n";
		}
	}
	return 0;
}
