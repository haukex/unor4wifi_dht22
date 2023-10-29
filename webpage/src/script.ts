/** Main TypeScript Source.
 *
 * This file is a part of: Arduino Uno R4 WiFi with DHT22 Sensor, https://github.com/haukex/unor4wifi_dht22
 *
 * This work © 2023 by Hauke Dämpfling (haukex@zero-g.net) is licensed under Attribution-ShareAlike 4.0
 * International. To view a copy of this license, visit http://creativecommons.org/licenses/by-sa/4.0/
 */

import { initialize_last_updated } from "./last-updated"

if (module.hot) { module.hot.accept() } // for parcel dev env

window.addEventListener('DOMContentLoaded', () => {

    // be fancy and make the formatting of last updated times dependent on screen width
    const lastupd_interv_ms = 500
    const narrow_mql = window.matchMedia('only screen and (max-width: 720px)')  // same condition as simple.css uses
    let lastupd_fmt :Intl.RelativeTimeFormat
    const init_last_upd = () => initialize_last_updated(lastupd_interv_ms, lastupd_fmt)
        const update_formatter = () => {
        lastupd_fmt = new Intl.RelativeTimeFormat('en', { style: narrow_mql.matches ? 'short' : 'long', numeric: 'auto' })
        init_last_upd()
    }
    narrow_mql.addEventListener('change', update_formatter)
    update_formatter()

    // get the various HTML elements
    const val_temp_c    = document.getElementById('val_temp_c'   ) as HTMLElement
    const val_humid_p   = document.getElementById('val_humid_p'  ) as HTMLElement
    const val_heatidx_c = document.getElementById('val_heatidx_c') as HTMLElement
    const val_age_ms    = document.getElementById('val_age_ms'   ) as HTMLElement
    const sensdata_err  = document.getElementById('sensdata_err' ) as HTMLElement
    const upd_enable    = document.getElementById('upd_enable'   ) as HTMLInputElement
    const upd_interv    = document.getElementById('upd_interv'   ) as HTMLInputElement
    const upd_now       = document.getElementById('upd_now'      ) as HTMLButtonElement

    // fetching sensor data
    let fetch_in_progress = false
    const fetch_sensordata = async () => {
        if (fetch_in_progress) return
        if (!document.hasFocus()) {
            //TODO Later: For long update intervals, it would be nice if the update
            // was performed immediately after the document gets focus again.
            console.debug("Skipping fetch b/c document does not have focus")
            return
        }
        fetch_in_progress = true
        try {
            upd_now.disabled = true
            const start = new Date()
            const resp = await fetch('/sensor.json', { signal: AbortSignal.timeout(4000) } )
            if (!resp.ok) throw new Error(`HTTP error! Status: ${resp.status}`)
            const sensdata = await resp.json()
            console.debug(sensdata)
            val_temp_c.innerText = sensdata['temp_c'].toFixed(1)
            val_humid_p.innerText = sensdata['humid_p'].toFixed(1)
            val_heatidx_c.innerText = sensdata['heatidx_c'].toFixed(1)
            // *approximate* the timestamp at which the data was taken
            val_age_ms.innerText = val_age_ms.title = new Date( start.getTime() - sensdata['age_ms'] ).toISOString()
            val_age_ms.classList.add("last-updated")
            init_last_upd()
            sensdata_err.classList.add("d-none")
        } catch (ex) {
            console.error(ex)
            sensdata_err.innerText = "❌ " + ex + (ex instanceof DOMException && ex.name == 'AbortError' ? ' (likely a timeout)' : '')
            sensdata_err.classList.remove("d-none")
        } finally {
            fetch_in_progress = false
            if ( !upd_enable.checked ) upd_now.disabled = false
        }
    }

    // options controlling the dynamic fetching of sensor data
    let fetch_timer_id :number = NaN
    const update_settings = () => {
        if ( !isNaN(fetch_timer_id) ) {
            console.debug("Stopping fetch timer")
            window.clearInterval(fetch_timer_id)
            fetch_timer_id = NaN
        }
        if ( upd_enable.checked ) {
            const upd_interv_ms = upd_interv.valueAsNumber*1000
            console.debug("Starting fetch timer, interval "+upd_interv_ms)
            fetch_timer_id = window.setInterval( fetch_sensordata, upd_interv_ms )
        }
    }
    upd_now.addEventListener('click', fetch_sensordata)
    upd_interv.addEventListener('change', update_settings)
    upd_enable.addEventListener('change', update_settings)
    upd_enable.addEventListener('change', () => {
        if ( upd_enable.checked ) {
            upd_now.disabled = true
        }
        else {
            if ( !fetch_in_progress ) upd_now.disabled = false
        }
    })
    update_settings()

    // the following is an attempt to prevent overloading the ardu with requests on page load (doesn't always work)
    window.addEventListener( 'load', () =>
        window.setTimeout( () => { if ( upd_enable.checked ) fetch_sensordata() }, 2000 ) )

    // This is just a demo / test of POST requests!
    const testpost_result = document.getElementById('testpost_result') as HTMLElement
    const btn_testpost = document.getElementById('testpost') as HTMLButtonElement
    btn_testpost.addEventListener('click', async () => {
        btn_testpost.disabled = true
        testpost_result.classList.remove("text-success", "text-danger")
        testpost_result.innerText = "…"
        try {
            const resp = await fetch('/testpost', {
                method: 'POST',
                headers: { 'Content-Type': 'application/octet-stream', },
                body: "Hi there",
                signal: AbortSignal.timeout(8000),
            })
            if (!resp.ok) throw new Error(`HTTP error! Status: ${resp.status}`)
            const txt = await resp.text()
            console.log(txt)
            testpost_result.innerText = "✔️ "+txt
            testpost_result.classList.add("text-success")
        } catch (ex) {
            console.error(ex)
            testpost_result.innerText = "❌ "+ex
            testpost_result.classList.add("text-danger")
        } finally {
            btn_testpost.disabled = false
        }
    })

})
