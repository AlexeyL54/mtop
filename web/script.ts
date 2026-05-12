/**
 * @fileoverview Frontend TypeScript для System Monitor
 * Получает и отображает системные метрики в реальном времени
 */

/**
 * Интерфейсы для типизации данных от API
 */

/** Данные о памяти */
interface MemoryData {
  MemTotal: number;      // Общая память в KB
  MemAvailable: number;  // Доступная память в KB
  MemFree: number;       // Свободная память в KB
  Cached: number;        // Кэшированная память в KB
}

/** Данные о ядре CPU */
interface CpuCore {
  id: number;    // ID ядра
  load: number;  // Загрузка в процентах (0-100)
}

/** Данные о CPU */
interface CpuData {
  total_cores: number;  // Общее количество ядер
  cores: CpuCore[];     // Массив с данными о каждом ядре
}

/** Данные о процессе */
interface ProcessInfo {
  pid: number;      // ID процесса
  name: string;     // Имя процесса
  state: string;    // Состояние: R, S, D, Z, T
  vmSize: number;   // Виртуальная память в KB
  vmRss: number;    // Физическая память (RSS) в KB
  threads: number;  // Количество потоков
}

/** Ответ от API процессов */
interface ProcessesResponse {
  processes: ProcessInfo[];  // Массив процессов
  total: number;             // Общее количество процессов
}

/** Константы приложения */
const UPDATE_INTERVAL_MS: number = 2000;  // Интервал обновления данных (2 секунды)
let updateInterval: number | null = null; // ID таймера для очистки при размонтировании

/**
 * Форматирует размер памяти из KB в читаемый вид
 * @param kb - Размер в килобайтах
 * @returns Отформатированная строка (например, "1.5 GB")
 */
function formatMemory(kb: number): string {
  if (kb >= 1024 * 1024) {
    return (kb / (1024 * 1024)).toFixed(2) + ' GB';
  } else if (kb >= 1024) {
    return (kb / 1024).toFixed(2) + ' MB';
  }
  return kb + ' KB';
}

/**
 * Обновляет отображение времени последнего обновления
 */
function updateTime(): void {
  const now: Date = new Date();
  const timeStr: string = now.toLocaleTimeString('en-US', {
    hour: '2-digit',
    minute: '2-digit',
    second: '2-digit'
  });

  const timeElement: HTMLElement | null = document.getElementById('update-time');
  if (timeElement) {
    timeElement.textContent = `Last update: ${timeStr}`;
  }
}

/**
 * Получает и отображает информацию о памяти
 */
async function updateMemory(): Promise<void> {
  try {
    const response: Response = await fetch('/api/memory');
    const data: MemoryData = await response.json();

    const total: number = data.MemTotal || 0;
    const available: number = data.MemAvailable || 0;
    const free: number = data.MemFree || 0;
    const cached: number = data.Cached || 0;

    const used: number = total - available;
    const usedPercent: string = total > 0 ? (used / total * 100).toFixed(1) : '0';

    // Обновляем текстовые поля статистики памяти
    const memTotalEl: HTMLElement | null = document.getElementById('mem-total');
    const memAvailableEl: HTMLElement | null = document.getElementById('mem-available');
    const memFreeEl: HTMLElement | null = document.getElementById('mem-free');
    const memCachedEl: HTMLElement | null = document.getElementById('mem-cached');

    if (memTotalEl) memTotalEl.textContent = formatMemory(total);
    if (memAvailableEl) memAvailableEl.textContent = formatMemory(available);
    if (memFreeEl) memFreeEl.textContent = formatMemory(free);
    if (memCachedEl) memCachedEl.textContent = formatMemory(cached);

    // Обновляем прогресс-бар использования памяти
    const memBar: HTMLElement | null = document.getElementById('mem-usage-bar');
    const memPercentEl: HTMLElement | null = document.getElementById('mem-usage-percent');

    if (memBar) {
      memBar.style.width = usedPercent + '%';
      memBar.textContent = usedPercent + '%';
    }
    if (memPercentEl) memPercentEl.textContent = usedPercent;

  } catch (error) {
    console.error('Ошибка при получении данных о памяти:', error);
  }
}

/**
 * Получает и отображает информацию о CPU (все ядра)
 */
async function updateCPU(): Promise<void> {
  try {
    const response: Response = await fetch('/api/cpu');
    const data: CpuData = await response.json();

    if (!data.cores || data.cores.length === 0) {
      console.warn('Нет данных о CPU');
      return;
    }

    let totalLoad: number = 0;
    const coresList: HTMLElement | null = document.getElementById('cpu-cores-list');

    if (!coresList) return;

    // Очищаем контейнер перед добавлением новых данных
    coresList.innerHTML = '';

    // Создаём карточку для каждого ядра CPU
    data.cores.forEach((core: CpuCore) => {
      totalLoad += core.load;
      const coreDiv: HTMLDivElement = document.createElement('div');
      coreDiv.className = 'cpu-core';
      coreDiv.innerHTML = `
                <div class="core-id">Core ${core.id}</div>
                <div class="core-load">${core.load.toFixed(1)}%</div>
                <div class="core-bar">
                    <div class="core-bar-fill" style="width: ${Math.min(core.load, 100)}%"></div>
                </div>
            `;
      coresList.appendChild(coreDiv);
    });

    // Вычисляем и отображаем среднюю загрузку CPU
    const avgLoad: string = data.total_cores > 0 ? (totalLoad / data.total_cores).toFixed(1) : '0';
    const cpuAvgEl: HTMLElement | null = document.getElementById('cpu-avg');
    if (cpuAvgEl) {
      cpuAvgEl.textContent = avgLoad + '%';
    }

  } catch (error) {
    console.error('Ошибка при получении данных о CPU:', error);
  }
}

/**
 * Фильтрует список процессов по поисковому запросу
 * Скрывает строки, не соответствующие запросу
 */
function filterProcesses(): void {
  const searchInput: HTMLInputElement | null = document.getElementById('process-search') as HTMLInputElement | null;
  const searchTerm: string = searchInput?.value.toLowerCase() || '';

  const rows: NodeListOf<HTMLTableRowElement> = document.querySelectorAll('#processes-body tr');
  let visibleCount: number = 0;

  rows.forEach((row: HTMLTableRowElement) => {
    const nameCell: HTMLTableCellElement | null = row.querySelector('td:nth-child(2)');
    const pidCell: HTMLTableCellElement | null = row.querySelector('td:nth-child(1)');

    const name: string = nameCell?.textContent?.toLowerCase() || '';
    const pid: string = pidCell?.textContent || '';

    if (name.includes(searchTerm) || pid.includes(searchTerm)) {
      row.style.display = '';
      visibleCount++;
    } else {
      row.style.display = 'none';
    }
  });

  const processCountEl: HTMLElement | null = document.getElementById('process-count');
  if (processCountEl) {
    processCountEl.textContent = `${visibleCount} processes (${rows.length} total)`;
  }
}

/**
 * Получает и отображает список процессов
 * Показывает топ-100 процессов по использованию памяти
 */
async function updateProcesses(): Promise<void> {
  try {
    const response: Response = await fetch('/api/processes');
    const data: ProcessesResponse = await response.json();

    const tbody: HTMLTableSectionElement | null = document.getElementById('processes-body') as HTMLTableSectionElement | null;
    if (!tbody) return;

    tbody.innerHTML = '';

    // Сортируем процессы по использованию физической памяти (RSS)
    const processes: ProcessInfo[] = data.processes || [];
    processes.sort((a: ProcessInfo, b: ProcessInfo) => b.vmRss - a.vmRss);

    // Ограничиваем до 100 процессов, чтобы не перегружать браузер
    const displayProcesses: ProcessInfo[] = processes.slice(0, 100);

    displayProcesses.forEach((proc: ProcessInfo) => {
      const row: HTMLTableRowElement = tbody.insertRow();
      const stateClass: string = getStateClass(proc.state);

      row.innerHTML = `
                <td>${proc.pid}</td>
                <td style="font-family: monospace;">${escapeHtml(proc.name || '<unknown>')}</td>
                <td><span class="process-state ${stateClass}">${proc.state}</span></td>
                <td>${formatMemory(proc.vmSize)}</td>
                <td>${formatMemory(proc.vmRss)}</td>
                <td>${proc.threads}</td>
            `;
    });

    // Обновляем счетчик процессов
    const processCountEl: HTMLElement | null = document.getElementById('process-count');
    if (processCountEl) {
      processCountEl.textContent = `${displayProcesses.length} processes (${processes.length} total)`;
    }

    // Повторно применяем фильтрацию (сохраняем поисковый запрос)
    filterProcesses();

  } catch (error) {
    console.error('Ошибка при получении данных о процессах:', error);
    const tbody: HTMLElement | null = document.getElementById('processes-body');
    if (tbody) {
      tbody.innerHTML = '<td><td colspan="6" class="loading">Ошибка загрузки процессов</td></tr>';
    }
  }
}

/**
 * Экранирует HTML-символы
 * @param str - Входная строка
 * @returns Экранированная строка
 */
function escapeHtml(str: string): string {
  if (!str) return '';
  return str
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;')
    .replace(/'/g, '&#39;');
}

/**
 * Возвращает CSS класс для отображения состояния процесса
 * @param state - Символ состояния процесса (R, S, D, Z, T)
 * @returns Название CSS класса для стилизации
 */
function getStateClass(state: string): string {
  switch (state) {
    case 'R': return 'state-R';  // Running
    case 'S': return 'state-S';  // Sleeping
    case 'D': return 'state-D';  // Uninterruptible sleep
    case 'Z': return 'state-Z';  // Zombie
    case 'T': return 'state-T';  // Traced/Stopped
    default: return '';
  }
}

/**
 * Обновляет все системные метрики одновременно
 * (параллельное выполнение запросов для производительности)
 */
async function updateAll(): Promise<void> {
  await Promise.all([
    updateMemory(),
    updateCPU(),
    updateProcesses()
  ]);
  updateTime();  // Обновляем время после получения всех данных
}

/**
 * Сортирует таблицу процессов по указанной колонке
 * @param column - Индекс колонки для сортировки (0-5)
 */
function sortTable(column: number): void {
  const table: HTMLTableElement | null = document.getElementById('processes-table') as HTMLTableElement | null;
  if (!table) return;

  const tbody: HTMLTableSectionElement | null = table.querySelector('tbody');
  if (!tbody) return;

  const rows: HTMLTableRowElement[] = Array.from(tbody.querySelectorAll('tr'));

  // Определяем, является ли колонка числовой
  const isNumeric: boolean = column === 0 || column === 3 || column === 4 || column === 5;

  rows.sort((a: HTMLTableRowElement, b: HTMLTableRowElement) => {
    let aVal: string = a.cells[column]?.textContent || '0';
    let bVal: string = b.cells[column]?.textContent || '0';

    if (isNumeric) {
      // Извлекаем числа из форматированных строк (обрезаем единицы измерения)
      const aNum: number = parseFloat(aVal) || 0;
      const bNum: number = parseFloat(bVal) || 0;
      return bNum - aNum;  // Сортировка по убыванию
    }

    return aVal.localeCompare(bVal);  // Лексикографическая сортировка
  });

  // Перемещаем отсортированные строки обратно в tbody
  rows.forEach((row: HTMLTableRowElement) => tbody.appendChild(row));
}

/**
 * Инициализирует приложение:
 * - Загружает начальные данные
 * - Запускает периодическое обновление
 * - Настраивает обработчики событий (поиск, сортировка)
 */
function init(): void {
  console.log('Инициализация System Monitor...');

  // Первоначальная загрузка данных
  updateAll();

  // Запускаем периодическое обновление
  updateInterval = window.setInterval(updateAll, UPDATE_INTERVAL_MS);

  // Настройка обработчика поиска
  const searchInput: HTMLInputElement | null = document.getElementById('process-search') as HTMLInputElement | null;
  if (searchInput) {
    searchInput.addEventListener('input', filterProcesses);
  }

  // Настройка обработчиков сортировки для всех заголовков таблицы
  const headers: NodeListOf<HTMLTableCellElement> = document.querySelectorAll('th');
  headers.forEach((header: HTMLTableCellElement, index: number) => {
    header.style.cursor = 'pointer';
    header.addEventListener('click', () => sortTable(index));
  });
}

/**
 * Очистка ресурсов при закрытии страницы
 * Отменяет таймер обновления, чтобы избежать утечек памяти
 */
function cleanup(): void {
  if (updateInterval !== null) {
    clearInterval(updateInterval);
    updateInterval = null;
  }
}

// Проверяем состояние загрузки документа
if (document.readyState === 'loading') {
  // Если DOM ещё не готов, ждём события DOMContentLoaded
  document.addEventListener('DOMContentLoaded', init);
} else {
  // DOM уже загружен, запускаемся сразу
  init();
}

// Очищаем ресурсы при выгрузке страницы
window.addEventListener('beforeunload', cleanup);
