# Учебная реализация умных указателей

Проект проверяет шаблонные классы:

```cpp
template <class T, class Deleter = std::default_delete<T>>
class UniquePtr;

template <class T>
class SharedPtr;

template <class T>
class WeakPtr;
```

Стартовые alias-шаблоны находятся в `include/my_smart_ptr.hpp`. Заменяйте их
своими классами. Тесты используют только публичный интерфейс, совместимый с
соответствующими классами стандартной библиотеки.

## Сборка и запуск

Проверить текущую реализацию из `include/my_smart_ptr.hpp`:

```bash
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Независимо проверить сам набор тестов на стандартной библиотеке:

```bash
cmake -S . -B build-std -DSMART_PTR_USE_STD=ON
cmake --build build-std -j
ctest --test-dir build-std --output-on-failure
```

Запускать уровни по отдельности можно без CTest:

```bash
./build/smart_ptr_tests "[basic]"
./build/smart_ptr_tests "[advanced]"
./build/smart_ptr_tests "[exceptions]"
./build/smart_ptr_tests "[threads]"
./build/smart_ptr_tests
```

Для поиска утечек и неопределённого поведения:

```bash
cmake -S . -B build-san -DSMART_PTR_ENABLE_SANITIZERS=ON
cmake --build build-san -j
ctest --test-dir build-san --output-on-failure
```

Проверить гонки в атомарных счётчиках `SharedPtr`/`WeakPtr` с ThreadSanitizer:

```bash
cmake -S . -B build-tsan -DSMART_PTR_ENABLE_TSAN=ON
cmake --build build-tsan -j
ctest --test-dir build-tsan -R threads --output-on-failure
```

ASan/UBSan и TSan нельзя включать в одной сборке, поэтому для них используются
разные каталоги.

## Уровни

- `basic`: пустое состояние, observers, move/copy, `reset`, `release`, `swap`,
  `use_count`, `lock`, время жизни объекта и раскрутка стека при исключении.
- `advanced`: custom deleter, `UniquePtr<T[]>`, преобразования derived-to-base,
  aliasing constructor, создание `SharedPtr` из `WeakPtr` и корректная обработка
  ошибки выделения control block.
- `threads`: конкурентное копирование `SharedPtr`, уничтожение последних
  владельцев и конкурентный `WeakPtr::lock`.

Рекомендуемый порядок реализации: `UniquePtr` → базовый `SharedPtr` → `WeakPtr`
→ продвинутые конструкторы → атомарные счётчики.
