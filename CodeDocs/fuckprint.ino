#define OPENJCOMMAND Serial.print("<p>");
#define CLOSEJCOMMAND Serial.println("</p>");
#include <stdio.h>


void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);
  delay(1000);
}

int lastNode1 = 1;
int node1 = 1;
int lastNode2 = 8;
int node2 = 8;

String openJ = "<j>";
String closeJ = "</j>";
float voltage;
void loop() {


  digitalWrite(LED_BUILTIN, HIGH);  
  

  delay(2800);             
  node1++;
  node2++;
  if (node1 > 60)
  {
    node1 = 1;
  }         

if (node2 > 60)
{
  node2 = 1;
}


OPENJCOMMAND
Serial.print(">disconnect( ADC0," + String(lastNode2) + ")" );
CLOSEJCOMMAND
delay(2600);
OPENJCOMMAND
Serial.print(">connect(ADC0 ," + String(node2) + ")" );
CLOSEJCOMMAND
delay(800);
OPENJCOMMAND
Serial.print(">adc_get(0)" );
CLOSEJCOMMAND
delay(40);
char response[30];

for (int i = 0; i < 30; i++)
{
response[i] = ' ';

}
int idx = 0;
while(Serial.available()>0)
{
  uint8_t c = Serial.read();
  response[idx] = c;
  idx++;

  if (idx >= 30){
    break;
  }
  delay(5);


  // voltage = Serial.parseFloat();
  // Serial.println(voltage);
}
Serial.println(response);
Serial.print("num chars read = ");
Serial.println(idx);
Serial.flush();

  // Serial.printf ("%s- %d - %d%s", openJ, lastNode1, lastNode2, closeJ);
  // Serial.printf ("%s+ %d - %d%s", openJ, node1, node2, closeJ);

lastNode1 = node1;
lastNode2 = node2;
  

  digitalWrite(LED_BUILTIN, LOW);   
  
  //delay(300);                      
  
}
