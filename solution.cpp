#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <vector>
#include <fstream>

bool echo_enabled = false;

// Функция для настройки TTY устройства
bool configure_tty(int fd, size_t baud_rate, int flags, int size) {
    struct termios tty;
    if (tcgetattr(fd, &tty) != 0) {
        std::perror("Ошибка tcgetattr");
        return false;
    }

    // Установка скорости передачи данных
    cfsetospeed(&tty, baud_rate);
    cfsetispeed(&tty, baud_rate);

    if (flags % 2 == 1)
        tty.c_cflag &= ~PARENB;        // Без четности

    flags >> 1;
    if (flags % 2 == 1)
        tty.c_cflag &= ~CSTOPB;        // 1 стоп-бит

    tty.c_cflag &= ~CSIZE;      // Задание размера через параметры
    switch (size) {
        case 8:
            tty.c_cflag |= CS8;            // 8 бит данных
            break;
        case 7:
            tty.c_cflag |= CS7;            // 7 бит данных
            break;
        case 6:
            tty.c_cflag |= CS6;            // 6 бит данных
            break;
        case 5:
            tty.c_cflag |= CS5;            // 5 бит данных
            break;
        default:
            break;
    }

    tty.c_cflag &= ~CRTSCTS;       // Отключение аппаратного управления потоком
    tty.c_cflag |= CREAD | CLOCAL; // Включить приемник, игнорировать линии управления

    // Перевод в сырой (raw) режим ввода-вывода
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
    tty.c_oflag &= ~OPOST;

    // Настройка таймаутов чтения (блокировать, пока не появится хотя бы 1 байт)
    tty.c_cc[VMIN] = 1;
    tty.c_cc[VTIME] = 0;

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        std::perror("Ошибка tcsetattr");
        return false;
    }

    return true;
}

bool Compare(std::string command, std::string reg_command) {
    bool skipMode = false;

    int j = 0;
    for (int i = 0; i < command.size(); i++) {
        if (reg_command[j] == '*') {
            skipMode = true;
            
            // На случай, если пришлю множество * подряд, которые могут обозначать 0 символов
            while (reg_command[j] == '*') {
                // Если сравнение дошло до момента *, на котором команда завершилась, то сравнение пройдено
                if (++j == reg_command.size()) {
                    return true;
                }
            }
        }

        if (skipMode) {
            if (command[i] == reg_command[j]) {
                j++;
                skipMode = false;
            }

            continue;
        }

        // Скобки можно открыть только 1 раз, для символов [], которые означают 1 символ
        if (reg_command[j] == '[') {
            if (reg_command[++j] != ']') {
                return false;
            }

            j++;
            continue;
        }

//        std::cout << "comm itteration: " << i << std::endl;
//        std::cout << "reg itteration: " << j << std::endl;
//        std::cout << "reg_command[j] = " << reg_command[j] << std::endl;
//        std::cout << "command[j] = " << command[j] << std::endl;
        if (reg_command[j] == '.' || command[i] == reg_command[j]) {
            j++;
            continue;
        }

        // Если код дошел до данного места, значит, символ в пришедшем выражении не равен символу команды
        return false;
    }

    // Если иттератор j не дошел до конца выражения, значит выражение не совпадает с командой
    if (j != reg_command.size()) {
        return false;
    }

    return true;
}

// Парсер пришедшей строки. Парсер сделан из расчета именно на данную задачу
int parse_command(std::string clean_cmd) {
    // Необходимо проверить, что символы совпадают с какой-либо
    // командой из списка и, что совпадают не более, чем с одной командой

    // Учитывая небольшое число команд, пройдемся каждым из них по полученной выражению (или
    // строке)

    std::vector<std::string> commands;
    commands.push_back("AT");
    commands.push_back("ATE1");
    commands.push_back("ATE0");
    commands.push_back("ATI");
    commands.push_back("AT+COPS");
    commands.push_back("AT+CPIN");

    int count = 0;
    int found = -1;
    for (int i = 0; i < commands.size(); i++) {
        if (Compare(commands[i], clean_cmd)) {
            count++;
            found = i;
        }
    }

    // Если найдено больше одной команды, выдаем отдельную ошибку
    if (count > 1)
        return -2;

    return found;

}

// Функция обработки входящей AT-команды
std::string process_at_command(const std::string& cmd) {
    // Удаляем символы переноса строки для удобства парсинга
    std::string clean_cmd = cmd;
    clean_cmd.erase(clean_cmd.find_last_not_of("\r\n") + 1);

    if (clean_cmd.empty()) {
        return "";
    }

    std::cout << "[Получено]: " << clean_cmd << std::endl;

    std::string answer;

    int num_cmd = parse_command(clean_cmd);

    // AT - 0
    // ATE0 - 1
    // ATE1 - 2
    // ATI - 3
    // AT+COPS - 4
    // AT+CPIN - 5
    switch (num_cmd) {
    case 0:
        answer = "OK";
        break;
    case 1:
        echo_enabled = false;
        answer = "Echo enabled";
        break;
    case 2:
        echo_enabled = true;
        answer = "Echo disabled";
        break;
    case 3:
        answer = "C++ Virtual AT Server";
        break;
    case 4:
        answer = "There should be information about real internet provider";
        break;
    case 5:
        answer = "SIM card was virtualy changed";
        break;
    }

    if (answer.empty()) {
    // Если подходит более одной команды
    if (num_cmd == -2)
        answer = "ERROR. Can't determine command";
    else
    // Если команда неизвестна
        answer = "ERROR. Command was not found";
    }

    return answer;
}

int main(int argc, char* argv[]) {

    if (argc < 2) {
        std::cerr << "Не хватает агрументов. Необходимо указать устройство (например, /dev/ttyUSB0)\n";
        return 1;
    }

    std::ofstream output("output.csv");

    const char* tty_device = argv[1];
    
    // Открываем устройство в режиме чтения/записи
    int fd = open(tty_device, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        std::cerr << "Не удалось открыть устройство\n";
        return 1;
    }

    if (!configure_tty(fd, B115200, 3, 8)) {
        close(fd);
        return 1;
    }

    std::cout << "Сервер запущен на " << tty_device << ". Ожидание AT-команд...\n";

    std::string buffer;
    char ch;

    while (true) {
        // Читаем по одному байту
        ssize_t n = read(fd, &ch, 1);
        if (n > 0) {
            if (ch != '\r' && ch != '\n' && ch != '\0') {
                buffer += ch;
                if (echo_enabled)
                        write(fd, &ch, 1);
            } else {

                if (echo_enabled)
                    write(fd, "\r\n", 2);

                if (buffer.size() > 1) {
                    output << buffer << "=";
                    buffer += ch;
                    std::string response = process_at_command(buffer);
                    if (!response.empty()) {
                         output << response << ";" << std::endl;
                        response += "\r\n";
                        write(fd, response.c_str(), response.size());
                    }
                    buffer.clear();
                }
            }
        } else if (n < 0) {
            std::perror("Ошибка чтения");
            break;
        }
    }

    close(fd);
    return 0;
}