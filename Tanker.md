#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h> 
#include <math.h> 
#include "lodepng.h" 

typedef struct {
    int x, y;
} Point;

typedef struct {
    int centerX, centerY;
    int is_potential_tanker;
    Point* pixels;
    int count;
} ObjectInfo;

// Загрузка и сохранение PNG
unsigned char* load_png(const char* filename, unsigned int* width, unsigned int* height) {
    unsigned char* image = NULL;
    unsigned error = lodepng_decode32_file(&image, (unsigned*)width, (unsigned*)height, filename);
    if (error != 0) printf("error %u: %s\n", error, lodepng_error_text(error));
    return image;
}

void write_png(const char* filename, const unsigned char* image, unsigned width, unsigned height) {
    unsigned char* png = NULL;
    size_t pngsize = 0;
    unsigned error = lodepng_encode32(&png, &pngsize, image, (unsigned)width, (unsigned)height);
    if (error == 0) lodepng_save_file(png, pngsize, filename);
    if (png) free(png);
}

// Расчет яркости пикселя (энергетический метод)
unsigned char get_luma(unsigned char* picture, int x, int y, int width) {
    int idx = (y * width + x) * 4;
    float r = (float)picture[idx], g = (float)picture[idx + 1], b = (float)picture[idx + 2];
    float energy = (r * r + g * g + b * b) / (3.0f * 150.0f);
    return (energy > 255) ? 255 : (unsigned char)energy;
}

// Проверка геометрии объекта (линейность для танкеров)
int check_if_linear(Point* pixels, int count, int minX, int maxX, int minY, int maxY) {
    if (count == 1) return 1; // Одиночный пиксель считаем потенциальным танкером
    if (count > 3) return 0;  // Слишком большие объекты — в мусор

    Point p1 = pixels[0], p2 = pixels[count - 1];
    for (int i = 0; i < count; i++) {
        float dist = (float)abs((p2.y - p1.y) * pixels[i].x - (p2.x - p1.x) * pixels[i].y + p2.x * p1.y - p2.y * p1.x) /
            (sqrt(pow(p2.y - p1.y, 2) + pow(p2.x - p1.x, 2)) + 0.00001f);
        if (dist > 2) return 0; // Если пиксели не в ряд
    }
    return 1;
}

// Поиск и группировка пикселей объекта (BFS)
ObjectInfo scan_and_classify(unsigned char* picture, unsigned char* visited, int sx, int sy, int width, int height, int threshold) {
    Point* stack = (Point*)malloc(width * height * sizeof(Point));
    Point* object_pixels = (Point*)malloc(width * height * sizeof(Point));
    int top = 0, count = 0;
    int minX = sx, maxX = sx, minY = sy, maxY = sy;

    stack[top++] = (Point){ sx, sy };
    visited[sy * width + sx] = 1;

    while (top > 0) {
        Point p = stack[--top];
        object_pixels[count++] = p;
        if (p.x < minX) minX = p.x; if (p.x > maxX) maxX = p.x;
        if (p.y < minY) minY = p.y; if (p.y > maxY) maxY = p.y;

        int dx[] = { 0, 0, 1, -1 }, dy[] = { 1, -1, 0, 0 };
        for (int i = 0; i < 4; i++) {
            int nx = p.x + dx[i], ny = p.y + dy[i];
            if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                int n_idx = ny * width + nx;
                if (!visited[n_idx] && get_luma(picture, nx, ny, width) > threshold) {
                    visited[n_idx] = 1;
                    stack[top++] = (Point){ nx, ny };
                }
            }
        }
    }
    ObjectInfo info = { (minX + maxX) / 2, (minY + maxY) / 2, 0, object_pixels, count };
    info.is_potential_tanker = check_if_linear(object_pixels, count, minX, maxX, minY, maxY);
    free(stack);
    return info;
}

// Расчет средней яркости фона в радиусе
float get_avg_luma(unsigned char* picture, int cx, int cy, int width, int height, int radius) {
    long long sum = 0; int count = 0;
    for (int y = cy - radius; y <= cy + radius; y++) {
        for (int x = cx - radius; x <= cx + radius; x++) {
            if (x >= 0 && x < width && y >= 0 && y < height) {
                sum += get_luma(picture, x, y, width);
                count++;
            }
        }
    }
    return (float)sum / count;
}

// Проверка: одинок ли объект в море или это часть суши/текста
int is_isolated_tanker(unsigned char* picture, ObjectInfo obj, int width, int height) {
    int total_bright = 0;
    int r = 2; // Минимальный радиус, чтобы игнорировать текст в паре пикселей

    for (int y = obj.centerY - r; y <= obj.centerY + r; y++) {
        for (int x = obj.centerX - r; x <= obj.centerX + r; x++) {
            if (x >= 0 && x < width && y >= 0 && y < height) {
                if (get_luma(picture, x, y, width) > 15.0f) total_bright++;
            }
        }
    }
    // pure_ratio: какую часть яркого пятна занимает сам объект
    float pure_ratio = (float)obj.count / (float)total_bright;
    return (pure_ratio > 0.2f); 
}

void color_object(unsigned char* result, ObjectInfo obj, int width, unsigned char r, unsigned char g, unsigned char b) {
    for (int i = 0; i < obj.count; i++) {
        int idx = (obj.pixels[i].y * width + obj.pixels[i].x) * 4;
        result[idx] = r; result[idx + 1] = g; result[idx + 2] = b; result[idx + 3] = 255;
    }
}

int main() {
    unsigned int width, height;
    unsigned char* picture = load_png("map.png", &width, &height);
    if (!picture) return -1;

    unsigned char* result_img = (unsigned char*)calloc(width * height * 4, sizeof(unsigned char));
    unsigned char* visited = (unsigned char*)calloc(width * height, sizeof(unsigned char));
    int tanker_count = 0, other_count = 0, threshold = 30;

    for (int y = 0; y < (int)height; y++) {
        for (int x = 0; x < (int)width; x++) {
            if (get_luma(picture, x, y, width) > threshold && !visited[y * width + x]) {
                ObjectInfo obj = scan_and_classify(picture, visited, x, y, width, height, threshold);

                // Средняя яркость в большом радиусе (30) для отсечения берега
                float avg30 = get_avg_luma(picture, obj.centerX, obj.centerY, width, height, 30);
                int is_real_tanker = 0;

                // 1. Если вокруг слишком светло (avg30 > 55) — это точно земля
                if (avg30 <= 55.0f) {
                    // 2. Танкерами считаем только мелкие объекты (1-3 пикселя)
                    if (obj.count <= 3) {
                        // 3. Проверка на изоляцию (чтобы не собрать мелкий мусор у берега или куски текста)
                        if (is_isolated_tanker(picture, obj, width, height)) {
                            is_real_tanker = 1;
                        }
                    }
                }

                if (is_real_tanker) {
                    tanker_count++;
                    color_object(result_img, obj, width, 0, 255, 0); // Зеленый
                } else {
                    other_count++;
                    color_object(result_img, obj, width, 255, 0, 0); // Красный
                }
                free(obj.pixels);
            }
        }
    }

    printf("Tankers (Green): %d\nOthers (Red): %d\n", tanker_count, other_count);
    write_png("debug_result.png", result_img, width, height);

    free(picture); free(visited); free(result_img);
    system("pause");
    return 0;
}
/*
Скажем прямо, я за это время забыл, что нужно загружать и как это делать, так что я оставлю комментарий по поводу всех проблем (не технических с которыми я столкнулся)
Разработка алгоритма началась с базовой задачи выделения ярких объектов на фоне океана с помощью поиска в ширину (BFS), но сразу столкнулась с проблемой избыточности данных,
когда за танкеры принимались любые светлые участки: от пены и облаков до целых горных хребтов. Для решения этой проблемы была внедрена концепция «трех сеток» фильтрации.
Первая сетка выполняла роль глобального сита, анализируя среднюю яркость в большом радиусе (30 пикселей) и отсекая массивные участки суши. Вторая сетка была сосредоточена на геометрии:
она проверяла линейность объектов, пропуская только те, что состояли из нескольких пикселей, вытянутых в ряд, что характерно для формы судна. Третья сетка занималась тонкой настройкой,
сравнивая локальную яркость объекта с яркостью ближайшего окружения, чтобы убедиться, что судно находится в открытом море, а не является частью прибрежного рельефа.
Однако в процессе тестирования выяснилось, что жесткая геометрическая проверка и широкие радиусы анализа создают «мертвые зоны» вокруг картографических надписей.
Текст координат и названий портов, наложенный поверх изображения, создавал колоссальный световой шум. При использовании стандартных радиусов в 10–15 пикселей, буквы «слипались»
с танкерами или задирали средний фон настолько, что алгоритм считал эти участки берегом и отправлял их в брак. Это привело к пересмотру всей архитектуры в пользу экстремальной локализации.
Финальная итерация алгоритма отошла от громоздких многослойных проверок в пользу высокочувствительного анализа изоляции. Вместо поиска «идеальной формы» на больших дистанциях, фокус
сместился на микро-радиус в 2 пикселя. Это позволило алгоритму игнорировать символы текста, находящиеся всего в паре шагов от судна. В сочетании с адаптивным коэффициентом чистоты
(pure_ratio), который теперь требовал доминирования яркости объекта в крошечном квадрате 5x5, удалось добиться разделения танкеров и элементов интерфейса. В итоге система эволюционировала
от грубого макро-сканирования к прецизионному фильтру, который видит одиночные пиксели судов даже в условиях сильных искусственных помех, что позволило увеличить число обнаруженных целей
в несколько раз.
Result:
Tankers (Green): 199
Others (Red): 678
*/
