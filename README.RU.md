[![EN](https://user-images.githubusercontent.com/9499881/33184537-7be87e86-d096-11e7-89bb-f3286f752bc6.png)](https://github.com/r57zone/Razer-Hydra-SteamVR-driver) 
[![RU](https://user-images.githubusercontent.com/9499881/27683795-5b0fbac6-5cd8-11e7-929c-057833e01fb1.png)](https://github.com/r57zone/Razer-Hydra-SteamVR-driver/blob/master/README.RU.md) 
# Razer Hydra Driver для SteamVR
Драйвер эмулирует Valve Index или HTC Vive контроллеры, с помощью контроллеров Razer Hydra. Тип контроллеров переключается в настройках. Поддерживается нажатие изменяемой кнопки клавиатуры и приседания. 

![](https://user-images.githubusercontent.com/9499881/191363340-17717ab4-8825-4904-aadd-1b8f606df23a.gif) ![](https://user-images.githubusercontent.com/9499881/191363360-531c6bc8-e294-43f4-a523-79aa4a6bb16e.gif)

## Раскладка Index контроллеров
Razer Hydra | Левый Index контроллер | Правый Index контроллер
------------ | ------------- | -------------
Кнопка 1 | Кнопка A | Кнопка захвата (Grip)
Кнопка 3 | Кнопка B | Приседание
Кнопка 2, бампер | Кнопка захвата (Grip) | Кнопка A
Кнопка 4 | Нажатие тачпада правого контроллера | Кнопка B
Кнопка старт | Системная кнопка | Системная кнопка

### Режимы стика
Режим работы стика и тачпада | Горячая клавиша
------------ | -------------
Стандартный режим, тачпад не эмулируется. | `ALT` + `1`
Тачпад эмулируется стиком, стик отключён. | `ALT` + `2`
Тачпад дублируется стик. | `ALT` + `3`
Тачпад эмулируется стиком, нажатия на dpad left и right, на правом контроллере, инвертированы. | `ALT` + `4`
Тачпад эмулируется стиком, нажатия на dpad up и up, на правом контроллере, инвертированы. | `ALT` + `5`

## Раскладка Vive контроллеров
Razer Hydra | Левый Vive контроллер | Правый Vive контроллер
------------ | ------------- | -------------
Кнопка 1 | Кнопка меню | Кнопка захвата (Grip)
Кнопка 3 | Нажатие dpad down на правом контроллере | Приседание
Кнопка 2, бампер | Кнопка захвата (Grip) | Кнопка меню
Кнопка 4 | Нажатие изменяемой кнопки клавиатуры, по умолчанию `V` | Нажатие dpad up на правом контроллере.
Кнопка старт | Системная кнопка | Системная кнопка

### Режимы стика
Режим работы стика и тачпада | Горячая клавиша
------------ | -------------
Стандартный режим. | `ALT` + `1`
Нажатия на dpad left и right на правом контроллере инвертированы. | `ALT` + `2`
Все нажатия инвертированы, кроме dpad up и up на правом контроллере. | `ALT` + `3`
Все нажатия инвертированы. | `ALT` + `4`

### Остальные особенности
Описание | Razer Hydra кнопка
------------ | -------------
Включение, выключение приседания | `ALT` + `9` и `ALT` + `0` (заменяет на нажатие тачпада)

- Для HMD можно использовать любой драйвер, с поддержкой приседания по кнопке. Например, можно использовать [OpenVR-ArduinoHMD драйвер](https://github.com/r57zone/OpenVR-ArduinoHMD) или [TrueOpenVR и SteamVR мост драйвер](https://github.com/TrueOpenVR) для HMD (FreeTrack для HMD из смартфонов или ArduinoHMD для [полноценных DIY шлемов](https://github.com/TrueOpenVR/TrueOpenVR-DIY/blob/master/HMD/HMD.RU.md)). По умолчанию это кнопка `PAUSE`, изменить её можно в конфигурационном файле "default.vrsettings", параметр `CrouchPressKey`, название нужной кнопки можно найти [здесь](https://github.com/r57zone/DualShock4-emulator/blob/master/BINDINGS.RU.md).

- Изменить тип контроллеров, с Valve Index на HTC Vive, можно изменив значение `true` на `false`, параметра `IndexControllers`, в конфигурационном файле "default.vrsettings", параметр `CustomPressKey`

- Во время нажатия кнопки 3, на правом контроллере Razer Hydra, также нажимается кнопка клавиатуры (кнопка настраивается). Настройки приседания можно найти в конфигурационном файле "default.vrsettings".

- Поддерживается нажатие кнопки клавиатуры, на кнопку 4, левого контроллера. По умолчанию это кнопка `V`, изменить её можно в конфигурационном файле "default.vrsettings", параметр `CustomPressKey`, название нужной кнопки можно найти [здесь](https://github.com/r57zone/DualShock4-emulator/blob/master/BINDINGS.RU.md). Включить её можно в конфигурационном файле, изменив значение `false` на `true`, параметра `EnableCustomKey` и она заменит нажатие тачпада контроллера.

- Отредактировать раскладку контроллеров можно также в "SteamVR Bindings UI", открыв настройки SteamVR, выбрав "Advance Settings" -> "Show" и перейдя в пункт контроллеры.

## Установка
1. [Загрузите](https://github.com/r57zone/Razer-Hydra-SteamVR-driver/releases/) последний драйвер.
2. Распакуйте архив в "..\Steam\steamapps\common\SteamVR\drivers".
3. [Добавьте параметр](https://youtu.be/QCA3m4_3IJM?t=197) `"activateMultipleDrivers" : true,` в конфиг "...\Steam\config\steamvr.vrsettings", в раздел `steamvr`.
4. Измените мёртвую зону, если ваш стик уходит в сторону, в конфиге "..\Steam\steamapps\common\SteamVR\drivers\razer_hydra\hydra\resources\settings\default.vrsettings", параметр `JoyStickDeadZone`. Чтобы определить значение мёртвой зоны, для проблемного стика, можно использовать [эту программу](https://github.com/r57zone/Sixence-Razer-Hydra-sample/releases).

## Решение проблем
**• Стик наклонен в одну из сторон и не двигается в протиположную**<br>
Закройте SteamVR, отключите USB провод контроллеров, подождите 5-10 секунд и подключите снова.


**• Драйвер не работает:**
1. Удалите предыдущий установленный драйвер в Steam или папку.
2. Загрузите [утилиту MotionCreator](https://github.com/r57zone/Razer-Hydra-SteamVR-driver/releases/tag/1) (официальная утилита от Sixence), переключите "Controller Mode" в режим "Motion controller".
3. Удалите MotionCreator.

Если не помогло попробуйте еще утилиту RazerHydra [[1]](https://support.razer.com/console/razer-hydra)[[2]](https://github.com/r57zone/Razer-Hydra-SteamVR-driver/releases/tag/1) (официальная утилита от Razer).


**• Двигается курсор**<br>
Удалите MotionCreator или RazerHydra утилиту.


**• Контроллеры безумно вращаются когда отодвигаешь их от базовой станции [(как здесь)](https://twitter.com/r57zone/status/1467868099940691970)**<br>
Контакты главной катушки, идущие в схему, окислились и их нужно зачистить, поцарапать или припаять напрямую без коннектора. 

## Сборка
1. Загрузите исходники и распакуйте.
2. [Загрузите "openvr"](https://github.com/ValveSoftware/openvr) и распакуйте в "C:\openvr".
3. [Загрузите "SixenseSDK_102215.zip"](https://github.com/r57zone/Razer-Hydra-SteamVR-driver/releases/tag/1) и распакуйте в "C:\SixenseSDK_102215".
4. [Загрузите Microsoft Visual Studio Code 2017](https://code.visualstudio.com/download) и скомпилируйте.
5. Измените в свойствах проекта версию SDK, а также набор инструментов на ваши, после чего выберите типа сборки "Release" а архитектуру "x86" или "x64".