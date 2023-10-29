/** Display Dynamic Last-Updated Times.
 *
 * Run ``initialize_last_updated(INTERVAL_MS)`` to start live updating all HTML elements that have
 * ``class="last-updated"`` and ``title="DATETIME_VALUE"``, where *DATETIME_VALUE* must be parseable by ``Date.parse()``.
 * If you add/remove HTML elements from the document, you may safely run the function again to re-initialize.
 *
 * This work © 2023 by Hauke Dämpfling (haukex@zero-g.net) is licensed under Attribution-ShareAlike 4.0
 * International. To view a copy of this license, visit http://creativecommons.org/licenses/by-sa/4.0/
 */

const SECOND_MS = 1000
const MINUTE_MS = SECOND_MS*60
const HOUR_MS   = MINUTE_MS*60
const DAY_MS    = HOUR_MS*24
const WEEK_MS   = DAY_MS*7
const MONTH_MS  = DAY_MS*30
const YEAR_MS   = DAY_MS*356

const _last_updated_timer_ids :number[] = []
export function stop_last_updated() {
    while (_last_updated_timer_ids.length) window.clearInterval( _last_updated_timer_ids.shift() )
}
export function initialize_last_updated(
        interval_ms :number = 10000,
        formatter :Intl.RelativeTimeFormat = new Intl.RelativeTimeFormat('en', { style: 'long', numeric: 'auto' })
    ) {
    stop_last_updated()
    Array.from<HTMLElement>( document.querySelectorAll('.last-updated') ).forEach( element => {
        const title = element.getAttribute('title')
        if (!title) { console.error("last-updated: No title attribute on "+element.outerHTML); return }
        const dt_ms = Date.parse(title)
        if (!dt_ms) { console.error("last-updated: Failed to parse title as Date: "+title); return }
        const upd = () => {
            const diff_ms = dt_ms - Date.now()
            const abs_diff_ms = Math.abs(diff_ms)
            if      ( abs_diff_ms>=YEAR_MS   ) element.innerText = formatter.format(Math.round(diff_ms/YEAR_MS),   "year"  )
            else if ( abs_diff_ms>=MONTH_MS  ) element.innerText = formatter.format(Math.round(diff_ms/MONTH_MS),  "month" )
            else if ( abs_diff_ms>=WEEK_MS   ) element.innerText = formatter.format(Math.round(diff_ms/WEEK_MS),   "week"  )
            else if ( abs_diff_ms>=DAY_MS    ) element.innerText = formatter.format(Math.round(diff_ms/DAY_MS),    "day"   )
            else if ( abs_diff_ms>=HOUR_MS   ) element.innerText = formatter.format(Math.round(diff_ms/HOUR_MS),   "hour"  )
            else if ( abs_diff_ms>=MINUTE_MS ) element.innerText = formatter.format(Math.round(diff_ms/MINUTE_MS), "minute")
            else                               element.innerText = formatter.format(Math.round(diff_ms/SECOND_MS), "second")
        }
        _last_updated_timer_ids.push( window.setInterval(upd, interval_ms) )
        upd()
    })
}
