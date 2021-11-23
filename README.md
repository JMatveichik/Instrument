# Instrument
Динамически подключаемая библеототек для управления спектрографом НЕСВИТ при помощи ПЛК ОВЕН  по протоколу Modbus TCP 

## Добавлено 

### 19.11.2021  версия 1.0.6.38
 Требования заказчика
 1. Добавить в функцию GetZero: 
>  Функции GetZero добавить в конце, чтобы он ждал выставления бита ICMD_REQ, после того когда он его сбросил. 
А затем вывести сообщение "Калибровка угла решетки проша успешно." 

Добавлен функционал в сответствии с требованием.
    
  
## Исправлено

### 19.11.2021  версия 1.0.6.38


## Удалено