# Web-Client-with-REST-API

PCOM - TEMA 4
Neagoe Mario-Alexandru - 324CC

Am ales sa folosesc nlohmann deoarece:
1) Este pentru C++
2) Usurinta cu care pot sa accesez date
3) Mi-a fost usor sa pot folosi prompturi (acele to_complete din cod)
pentru diverse comenzi. Prin aceste prompturi mi-a fost usor sa
preiau datele de la tastatura/utilizator si sa le gestionez.
In codul meu:
- am inclus acel json.hpp
- am declarat namespace jason = nlohmann;
- am folosit jason::ordered_json ca sa creez obiectele si sa le gestionez
+ ca ordered_json mentine ordinea campurilor care trebuie completate de
utilizator, un pic paranoia din partea mea dar mai bine sa fiu sigur
- am folosit functia basic_extract_json_response ca sa extrag si sa parsez
mesajul din raspunsul HTTP
- am folosit ca sa trimit cereri si sa primesc raspunsuri de la server,
deoarece functiile care fac asta folosesc acele obiecte json

Nu sunt 100% sigur daca sunt sau nu probleme cu checker-ul oferit de
dumneavoastra (am avut niste probleme, dar le-am reparat), dar manual
merge perfect.

Am gestionat urmatoarele cazuri:
- cand un utilizator vrea sa intre in bibilioteca dar nu e logat
- cand un utilizator vrea sa se logheze dar e deja logat
- vrea sa inregistreze un cont, dar e logat deja
- vrea sa faca add book, delete book, get book si get books fara
sa fi facut enter library, adica cand nu are tokenul jwt
- daca la inregistrare si logare foloseste spatii
- daca cand face delete book sau get book, id-ul nu este numar
- cand trebuie sa faca add book, daca la autor, titlu, publisher, gen
nu introduce nimic
- cand primesc o comanda care nu exista

De multe ori, cand am o 'eroare', afisez raspunsul server-ului,
din moment ce oricum avea cuvantul 'error' in el si era detectat
de checker.

In cazurile pozitive afisez Succes + mesaj. Mesajele de succes nu
sunt de la server, sunt afisate folosind cout-uri de catre mine.

In main am folosit un loop ca sa verific ce comanda trebuie apelata,
cu un if pentru fiecare comanda si o variabila okay ca sa vad daca am
o comanda valida sau trebuie sa afisez ca nu am primit ceva valid.
Am vazut aceasta modalitate de a selecta ce functie sa apelez ca cea mai
straightforward varianta si usoara de implementat pentru mine.

In general, functiile au mers pe acelasi principiu dupa un punct:
- faceam in mod normal ce era specific pentru fiecare functie
(de ex: la get book, formam path-ul pentru HTTP header cu id-ul
dat de utilizator, asa ca extrageam id-ul)
- faceam mesajul HTTP
- deschideam conexiunea si trimiteam la server
- primeam raspunsul de la server
- gestionam raspunsul (ex: la logout, afisam ca s-a delogat cu succes
si stergeam cookieul si tokenul jwt, la register pur si simplu afisam ca
s-a inregistrat)
- inchid conexiunea
- eliberez memoria mesajului si a raspunsului de la server

Functiile care nu au fost cerute explicit sunt check response
si http header, pe care le-am explicat prin comentarii. Si functia
check prompts.

In functia check prompts gestionez datele introduse de utilizator.
Aici fac afisarea in terminal pentru campuri (ex: "id="), aici
citesc datele din stdin cu fgets, daug terminator de sir, verific sa
nu am input gol, verific ca page count sa fie numar, verific ca author,
title, genre, publisher sa nu fie campuri goale si completez obiectul json
cu datele primite de la utilizator, daca au fost verificate prin pasii de mai
sus.




