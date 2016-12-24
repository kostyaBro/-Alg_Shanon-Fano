#include <QByteArray>
#include <QDebug>
#include <QFile>
#include <QList>
#include <QMap>
#include <QPair>
#include <QString>
#include <QtAlgorithms>
#include <QTextStream>

#include <cmath>
#include <cstdio>

typedef QPair<char, int> Pair; //Пара сивол-частота
typedef QList<bool> CodeList; //Список нулей/единиц

enum Branch //"Направление" ветки дерева (левая/правая ветка)
{
    Left = 1,
    Right
};

static bool reverseLessThan(const Pair &x, const Pair &y) //Сравнение пары симол-частота
{
    return x.second >= y.second; //Считаем, что чем больше частота, тем ближе к началу списка должна быть пара
}

static void writeLine(const QString &text = QString()) //Запись в консоль
{
    static QTextStream writeStream(stdout, QIODevice::WriteOnly);
    writeStream << text << "\n";
    writeStream.flush();
}

static QByteArray readFile(const QString &fileName, int &errorCode) //Чтение файла
{
    QFile f(fileName);
    if (!f.exists()) {
        writeLine("File \"" + f.fileName() + "\" does not exist"); //Файл не существует
        errorCode = 2;
        return QByteArray();
    }
    if (!f.open(QFile::ReadOnly)) {
        writeLine("Unable to open file \"" + f.fileName() + "\""); //Не удалось открыть
        errorCode = 3;
        return QByteArray();
    }
    QByteArray data = f.readAll();
    if (f.error() != QFile::NoError) {
        writeLine("Unable to read file \"" + f.fileName() + "\""); //При чтении произошла ошибка
        errorCode = 4;
        f.close();
        return QByteArray();
    }
    f.close();
    return data;
}

static int writeFile(const QString &fileName, const QByteArray &data) //Запись файла
{
    QFile f(fileName);
    if (!f.open(QFile::WriteOnly)) {
        writeLine("Unable to open file \"" + f.fileName() + "\""); //Не удалось открыть
        return 5;
    }
    f.write(data);
    if (f.error() != QFile::NoError) {
        writeLine("Unable to write file \"" + f.fileName() + "\""); //При чтении произошла ошибка
        f.close();
        return 6;
    }
    f.close();
    return 0;
}

static QByteArray bitsToBytes(const QList<bool> &bits) //Преобразуем список битов в массив байт
{                                                      //"Лишние" биты устанавливаются в значение 0
    QByteArray bytes;
    bytes.resize(bits.size() / 8);
    bytes.fill(0);
    for (int b = 0; b < bits.size(); ++b)
        bytes[b/8] = (bytes.at(b / 8) | ((bits.at(b) ? 1 : 0) << (b % 8)));
    return bytes;
}

static QList<bool> bytesToBits(const QByteArray &data) //Преобразуем массив байт в список битов
{
    QList<bool> bits;
    foreach (char c, data) {
        uchar mask = 1;
        for (int i = 0; i < 8; ++i) {
            bits << (uchar(c) & mask);
            mask = mask * 2;
        }
    }
    return bits;
}

static QByteArray zip(const QByteArray &sourceData, const QMap<char, CodeList> &codes) //Упаковываем
{
    QList<bool> list;
    foreach (char c, sourceData)
        list << codes.value(c);
    QByteArray data(1, 0); //Резервируем один байт для указания количества "лишних" битов
    data += bitsToBytes(list);
    data[0] = (data.size() - 1) * 8 - list.size(); //Указываем количество лишних битов
    return data;
}

static QByteArray unzip(const QByteArray &sourceData, const QMap<char, CodeList> &codes) //Распаковываем
{
    typedef QPair<CodeList, char> CodePair;
    QList<CodePair> codeList;
    foreach (char c, codes.keys())
        codeList << qMakePair(codes.value(c), c); //Создаем список пар код-символ из карты символ-код
    QList<bool> nlist = bytesToBits(sourceData.mid(1)); //Получаем список битов
    char extra = sourceData.mid(0, 1).at(0); //Получаем количество "лишних" битов
    while (extra > 0) { //Удаляем "лишние" биты
        nlist.removeLast();
        --extra;
    }
    QByteArray data;
    while (!nlist.isEmpty()) {
        foreach (const CodePair &p, codeList) { //Проходимся по списку пар код-символ, ищем совпадение кода в списке
            bool e = true;
            for (int i = 0; i < p.first.size(); ++i) {
                if (nlist.at(i) != p.first.at(i)) {
                    e = false; //Если хоть один бит не совпадает, прерываем цикл и ставим соответствующий флаг
                    break;
                }
            }
            if (e) { //Если флаг true, значит фрагмент совпал
                for (int i = 0; i < p.first.size(); ++i) //Удаляем совпавший фрагмент из списка
                    nlist.removeFirst();
                data += p.second; //Добавляем соответствующий символ в новый массив байт
                break;
            }
        }
    }
    return data;
}

static QList<Pair> buildList(const QByteArray &data) //Строим список пар символ-частота
{
    QMap<char, Pair> frequencies;
    foreach (char c, data) { //Проходимся по всем символам
        if (!frequencies.contains(c)) //Если символ еще не встречался, добавляем в словарь со значением частоты 1
            frequencies.insert(c, qMakePair(c, 1));
        else
            ++frequencies[c].second; //Если символ уже встречался, увеличиваем его частоту
    }
    QList<Pair> list = frequencies.values(); //Получаем все значения словаря (ключи нам больше не нужны)
    qSort(list.begin(), list.end(), &reverseLessThan); //Сортируем таким образом, чтобы наиболее часто встречающиеся
    return list;                                       //символы были ближе к началу списка
}

static int balancedMid(const QList<Pair> &list) //Ищем середину списка таким образом, чтобы суммарная частота символов
{                                               //в каждой половине была примерно одинаковой
    if (list.isEmpty())
        return -1;
    if (list.size() < 2)
        return 0;
    QMap<int, int> map;
    for (int i = 0; i < list.size() - 1; ++i) { //Заполняем словарь. Ключ - модуль разности суммарной частоты частей,
        int left = list.at(i).second;           //значение - позиция
        for (int j = 0; j < i; ++j)
            left += list.at(j).second;
        int right = list.at(i + 1).second;
        for (int j = i + 2; j < list.size(); ++j)
            right += list.at(j).second;
        map.insert(std::abs(left - right), i);
    }
    return map.values().first(); //Поскольку словарь автоматически отсортирован, просто берем первое значение
}

static QString toString(const CodeList &list) //Превращаем список нулей и единиц в строку
{
    QString s;
    foreach (bool b, list)
        s += b ? "1" : "0";
    return s;
}

static void shannonFanoRecursive(const QList<Pair> &list, QMap<char, CodeList> &codes, Branch branch,
                                 const CodeList &parent = CodeList()) //Рекурсивно вычисляем коды для каждого символа
{
    bool value = ((Left == branch) ? false : true); //Левая ветка - 0, правая - единица
    if (list.size() == 1) {
        codes.insert(list.first().first, CodeList() << parent << value); //Если в списке остался один символ,
        return;                                                          //вписываем его код в словарь. Код получается
    }                                                                    //сложением всех вышестоящих кодов + текущего
    int mid = balancedMid(list); //Находим середину списка
    //Добавляем текущий код ко всем предыдущим и запускаем эту же функцию для левой и правой частей списка
    shannonFanoRecursive(list.mid(0, mid + 1), codes, Left, CodeList() << parent << value);
    shannonFanoRecursive(list.mid(mid + 1), codes, Right, CodeList() << parent << value);
}

static void shannonFano(const QList<Pair> &list, QMap<char, CodeList> &codes) //Запускаем алгоритм
{
    //Поскольку у корня дерева нет своего кода, мы его не указываем
    int mid = balancedMid(list);
    shannonFanoRecursive(list.mid(0, mid + 1), codes, Left);
    shannonFanoRecursive(list.mid(mid + 1), codes, Right);
}

int main(int argc, char **argv)
{
    if (argc < 2) { //Должен быть передан входной параметр - путь к файлу
        writeLine("Usage: shannon-fano [file]...");
        return 1;
    }
    for (int i = 1; i < argc; ++i) {
        QString fileName = QString::fromLocal8Bit(argv[i]); //Аргументы передаются в нативной кодировке, преобразуем
        int errorCode = 0;
        QByteArray data = readFile(fileName, errorCode); //Читаем файл
        if (errorCode) //Если прочитать не удалось, завершаем программу
            return errorCode;
        if (data.size() < 2) { //Если размер файла меньше 2, завершаем программу (все равно нечего делать)
            writeLine("Nothing to do (file is empty)");
            return 0;
        }
        QList<Pair> list = buildList(data); //Строим список пар символ-частота
        QMap<char, CodeList> codes;
        shannonFano(list, codes); //Запускаем алгоритм. Результат будет в переменной codes (она передается по ссылке)
        writeLine("Shannon-Fano code for \"" + fileName + "\":"); //Выводим результат в stdout
        foreach (const Pair &p, list) {
            const char &c = p.first;
            //Если символ печатаемый (буква, цифра, знаки препинания, и т.д.), то выводим как есть.
            //В противном случае выводим код символа
            QString s = (c >= 33 && c <= 126) ? (QString("'") + QChar(c) + "'") : ("[" + QString::number(c) + "]");
            while (s.length() < 6)
                s.prepend(' ');
            s += ": " + toString(codes.value(c));
            writeLine(s);
        }
        writeLine();
        QByteArray zipped = zip(data, codes); //Запаковываем
        errorCode = writeFile(fileName + ".compressed", zipped); //Пишем запакованные данные в файл
        if (errorCode)
            return errorCode;
        zipped = readFile(fileName + ".compressed", errorCode); //Читаем запакованные данные из файла
        if (errorCode)
            return errorCode;
        QByteArray unzipped = unzip(zipped, codes); //Распаковываем
        errorCode = writeFile(fileName + ".uncompressed", unzipped); //Пишем распакованные данные в третий файл
        if (errorCode)                                               //Открываем его и сравниваем с исходным
            return errorCode;
    }
    return 0;
}
