See: https://nordicsemi.atlassian.net/wiki/spaces/SWLW/pages/142964330/Provisioning+devices+to+nRF+Cloud

Scripts from: https://github.com/NordicPlayground/nrf9x-device-provisioning

....

######################################

The old way:

- put desh in deactivated mode:
desh:~ dect deactivate

- get device uuid from desh

desh:~$ at at%deviceuuid
%DEVICEUUID: 50345053-3639-49c0-80fe-172000f3296d
OK

- use that UUID to create certs e.g. by using following scripts
jani@JAHI1:~/code_wa/utils/python/modem-firmware-1.3+$ python3 create_ca_cert.py -c FI -st Finland -l Tampere -o "Nordic Semiconductor" -ou "RD" -cn 50345053-3639-49c0-80fe-172000f3296d -p ~/certs/my_ca
File created: /home/jani/certs/my_ca/0x7854a4c068d2a408470b3d47bf63efc84d4d0c3b_ca.pem
File created: /home/jani/certs/my_ca/0x7854a4c068d2a408470b3d47bf63efc84d4d0c3b_prv.pem
File created: /home/jani/certs/my_ca/0x7854a4c068d2a408470b3d47bf63efc84d4d0c3b_pub.pem

jani@JAHI1:~/code_wa/utils/python/modem-firmware-1.3+$ python3 create_device_credentials.py -ca /home/jani/certs/my_ca/0x7854a4c068d2a408470b3d47bf63efc84d4d0c3b_ca.pem -ca_key /home/jani/certs/my_ca/0x7854a4c068d2a408470b3d47bf63efc84d4d0c3b_prv.pem -c FI -o Nordic -ou Cloud -dv 2000 -cn 50345053-3639-49c0-80fe-172000f3296d -p ~/certs/dev_credentials/dect-mac

File created: /home/jani/certs/dev_credentials/dect-mac/50345053-3639-49c0-80fe-172000f3296d_crt.pem
File created: /home/jani/certs/dev_credentials/dect-mac/50345053-3639-49c0-80fe-172000f3296d_pub.pem
File created: /home/jani/certs/dev_credentials/dect-mac/50345053-3639-49c0-80fe-172000f3296d_prv.pem

- export client cert to csv file:

$ export CSV_FILE_PATH=~/certs/50345053-3639-49c0-80fe-172000f3296d-provisioning.csv
$ echo -n "50345053-3639-49c0-80fe-172000f3296d,,,,\"" > $CSV_FILE_PATH
$ cat /home/jani/certs/dev_credentials/dect-mac/50345053-3639-49c0-80fe-172000f3296d_crt.pem >> $CSV_FILE_PATH
$ echo "\"" >> $CSV_FILE_PATH$ cat $CSV_FILE_PATH

- upload that to cloud in https://nrfcloud.com/#/add-device/bulk (there must be some REST api for this as well to automation?)

- your device should appear in devices list

- store certs&key to desh by using custom %CMNG command (implemented in desh at cmd mode) - either by using directly at command (desh:~$ at at%cmng=******) OR by usin nrf connect cellular monitor)

- cellular monitor eats json format with '\n' as a linebreaks, at command something else (\r\n???)

example of json:
{
	"clientId": "50345053-3639-49c0-80fe-172000f3296",
	"caCert": "-----BEGIN CERTIFICATE-----\nMIIDQTCCAimgAwIBAgITBmyfz5m/jAo54vB4ikPmljZbyjANBgkqhkiG9w0BAQsF\nADA5MQswCQYDVQQGEwJVUzEPMA0GA1UEChMGQW1hem9uMRkwFwYDVQQDExBBbWF6\nb24gUm9vdCBDQSAxMB4XDTE1MDUyNjAwMDAwMFoXDTM4MDExNzAwMDAwMFowOTEL\nMAkGA1UEBhMCVVMxDzANBgNVBAoTBkFtYXpvbjEZMBcGA1UEAxMQQW1hem9uIFJv\nb3QgQ0EgMTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALJ4gHHKeNXj\nca9HgFB0fW7Y14h29Jlo91ghYPl0hAEvrAIthtOgQ3pOsqTQNroBvo3bSMgHFzZM\n9O6II8c+6zf1tRn4SWiw3te5djgdYZ6k/oI2peVKVuRF4fn9tBb6dNqcmzU5L/qw\nIFAGbHrQgLKm+a/sRxmPUDgH3KKHOVj4utWp+UhnMJbulHheb4mjUcAwhmahRWa6\nVOujw5H5SNz/0egwLX0tdHA114gk957EWW67c4cX8jJGKLhD+rcdqsq08p8kDi1L\n93FcXmn/6pUCyziKrlA4b9v7LWIbxcceVOF34GfID5yHI9Y/QCB/IIDEgEw+OyQm\njgSubJrIqg0CAwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMC\nAYYwHQYDVR0OBBYEFIQYzIU07LwMlJQuCFmcx7IQTgoIMA0GCSqGSIb3DQEBCwUA\nA4IBAQCY8jdaQZChGsV2USggNiMOruYou6r4lK5IpDB/G/wkjUu0yKGX9rbxenDI\nU5PMCCjjmCXPI6T53iHTfIUJrU6adTrCC2qJeHZERxhlbI1Bjjt/msv0tadQ1wUs\nN+gDS63pYaACbvXy8MWy7Vu33PqUXHeeE6V/Uq2V8viTO96LXFvKWlJbYK8U90vv\no/ufQJVtMVT8QtPHRh8jrdkPSHCa2XV4cdFyQzR1bldZwgJcJmApzyMZFo6IQ6XU\n5MsI+yMRQ+hDKXJioaldXgjUkK642M4UwtBV8ob2xJNDd2ZhwLnoQdeXeGADbkpy\nrqXRfboQnoZsG4q5WTP468SQvvG5\n-----END CERTIFICATE-----\n",
	"privateKey": "-----BEGIN PRIVATE KEY-----\nMIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQg5IezKuhQ/P1jw/G/\nhTUs9J7i+0eKSQ60t6uZNrypdBOhRANCAAQcKQxzQwT6K1IokYfZ5JmpR62HzU/A\nrHBB+B7kuWCmIV41Rm+kWDX0wXEEfQ6CNO9RKgOcdJrPtT8gUZnzDEHa\n-----END PRIVATE KEY-----\n",
	"clientCert": "-----BEGIN CERTIFICATE-----\nMIIB5TCCAYsCFGeofi4n+ND2EvDbETcCWQbPaltWMAoGCCqGSM49BAMCMIGMMQsw\nCQYDVQQGEwJGSTEQMA4GA1UECAwHRmlubGFuZDEQMA4GA1UEBwwHVGFtcGVyZTEd\nMBsGA1UECgwUTm9yZGljIFNlbWljb25kdWN0b3IxCzAJBgNVBAsMAlJEMS0wKwYD\nVQQDDCQ1MDM0NTA1My0zNjM5LTQ5YzAtODBmZS0xNzIwMDBmMzI5NmQwHhcNMjUw\nNDEwMTEwMTAwWhcNMzAxMDAxMTEwMTAwWjBdMQswCQYDVQQGEwJGSTEPMA0GA1UE\nCgwGTm9yZGljMQ4wDAYDVQQLDAVDbG91ZDEtMCsGA1UEAwwkNTAzNDUwNTMtMzYz\nOS00OWMwLTgwZmUtMTcyMDAwZjMyOTZkMFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcD\nQgAEHCkMc0ME+itSKJGH2eSZqUeth81PwKxwQfge5LlgpiFeNUZvpFg19MFxBH0O\ngjTvUSoDnHSaz7U/IFGZ8wxB2jAKBggqhkjOPQQDAgNIADBFAiEA2R/QWmSFoMfH\n/rPY5f0S4wM9IpWdI9FBo9hXJXPLiR4CICT9vt7ZlIKQbRBytyYkXZ3kK5Dorog1\nNTLazuEB0b3Q\n-----END CERTIFICATE-----\n"
}

- then connect device to dect sink
- and connect to cloud:

desh:~$ cloud connect