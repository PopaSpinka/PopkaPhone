'use strict';
/*
 * Новая вкладка «Браузера» — та же, что у Naomi Web в НаомиОС (browser/ext/newtab.js): «Избранное» плитками —
 * закладки в порядке списка под звездой полосы — и «Часто»: сайты, где бывали за 30 дней, по числу заходов, кроме
 * тех, что уже в «Избранном». Разница — откуда данные: расширений у Qt WebEngine нет, поэтому закладки, «Часто» и
 * значки (файлы PNG профиля) программа кладёт сама — naomiNewtab(данные) при загрузке и когда вкладку снова
 * открыли (docs/WEB.md, «Новая вкладка»).
 *
 * Открываем сами: щелчок — сайт в этой вкладке; средняя кнопка, ⌘- и Ctrl-щелчок — в новой рядом, эта остаётся
 * открытой. «В новой» — строкой в консоль (naomi:open-bg адрес): её ловит программа, ссылки и window.open не нужны.
 * Правая кнопка — меню страницы НаомиОС: событие не отменяем, его ловит программа, как на сайтах.
 */
const TILE_ICON = 24; // значок сайта в плитке и в строке, точки CSS
const ROW_ICON = 16;
const GONE_MS = 160; // строка «Часто» сворачивается — потом программа забывает сайт и шлёт «Часто» заново
const AGAIN_MS = 400; // корзина после удаления спит: под мышью уже следующая строка (как у «Избранного» на столе)
// корзина — те же линии, что у значка trash стола (Glyph.qml), 14 точек и линия 1,3
const TRASH = ['-5.75,-3.25 5.75,-3.25', '-2.25,-3.25 -2.25,-5.25 2.25,-5.25 2.25,-3.25',
	'-4.5,-3.25 -3.75,5.25 3.75,5.25 4.5,-3.25', '-1.5,-0.75 -1.5,2.75', '1.5,-0.75 1.5,2.75'];

const $ = (id) => document.getElementById(id);

function icon(src, size) {
	const img = document.createElement('img');
	img.width = img.height = size;
	img.alt = '';
	if (src) img.src = src;
	else img.style.visibility = 'hidden';
	return img;
}

/* Сайт: имя хоста без www (у file:// и прочих без хоста — весь адрес). */
function site(url) {
	try {
		return new URL(url).hostname.replace(/^www\./, '') || url;
	} catch {
		return url;
	}
}

function link(className, url) {
	const a = document.createElement('a');
	a.className = className;
	a.dataset.url = url;
	return a;
}

function span(className, text) {
	const s = document.createElement('span');
	s.className = className;
	s.textContent = text;
	return s;
}

function trash() {
	const NS = 'http://www.w3.org/2000/svg';
	const svg = document.createElementNS(NS, 'svg');
	for (const [k, v] of Object.entries({ width: 14, height: 14, viewBox: '-7 -7 14 14', fill: 'none',
		stroke: 'currentColor', 'stroke-width': 1.3, 'stroke-linecap': 'round', 'stroke-linejoin': 'round' }))
		svg.setAttribute(k, v);
	for (const points of TRASH) {
		const line = document.createElementNS(NS, 'polyline');
		line.setAttribute('points', points);
		svg.append(line);
	}
	const s = span('drop', '');
	s.append(svg);
	return s;
}

/* data: {favorites: [{title, url, icon}], frequent: [{site, url, visits, icon}]} — считает программа */
window.naomiNewtab = (data) => {
	const favs = data.favorites.filter((b) => !/^javascript:/i.test(b.url));
	const often = data.frequent;

	$('fav-tiles').replaceChildren(...favs.map((b) => {
		const a = link('tile', b.url);
		a.append(icon(b.icon, TILE_ICON), span('name', b.title || site(b.url)));
		return a;
	}));
	$('fav-count').textContent = favs.length || '';
	$('fav-empty').hidden = favs.length > 0;

	$('freq-rows').replaceChildren(...often.map((s) => {
		const a = link('row', s.url);
		a.dataset.site = s.site;
		const end = span('end', '');
		end.append(span('count', String(s.visits)), trash());
		a.append(icon(s.icon, ROW_ICON), span('title', s.site), end);
		return a;
	}));
	$('frequent').hidden = often.length === 0;
};

/* ---------- Открыть ---------- */

function open(url, background) {
	if (background) console.log('naomi:open-bg ' + url);
	else location.href = url;
}

const target = (event) => event.target.closest('[data-url]');

/* Корзина: строка сворачивается, программа забывает сайт (вся его история) и шлёт «Часто» заново. Второй щелчок
 * двойного — мимо: под мышью уже следующая строка. По времени, а не по event.detail: счёт щелчков зависит от того,
 * как мышь дошла до движка (на стенде с QTest второй щелчок пришёл с detail 1 и стёр вторую строку). */
let forgotAt = -AGAIN_MS;
function forget(row) {
	if (performance.now() - forgotAt < AGAIN_MS) return;
	forgotAt = performance.now();
	row.classList.add('gone'); // высота строки задана (--row-h) — переход к 0 идёт сразу
	setTimeout(() => console.log('naomi:forget ' + row.dataset.site), GONE_MS);
}

addEventListener('click', (event) => {
	const a = event.button === 0 && target(event);
	if (!a) return;
	event.preventDefault();
	if (event.target.closest('.end')) {
		forget(a);
		return;
	}
	open(a.dataset.url, event.ctrlKey || event.metaKey || event.shiftKey);
});
addEventListener('auxclick', (event) => {
	const a = event.button === 1 && target(event);
	if (!a) return;
	event.preventDefault();
	if (!event.target.closest('.end')) open(a.dataset.url, true);
});
addEventListener('mousedown', (event) => {
	if (event.button === 1) event.preventDefault(); // средняя кнопка — без прокрутки «колёсиком»
});
