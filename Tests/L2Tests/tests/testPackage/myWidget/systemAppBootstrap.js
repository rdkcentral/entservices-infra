async function checkUrlBeforeLoading(url) {
    try {
        const response = await fetch(url, {
            method: "HEAD"
        });

        if (response.ok) {
            console.log('Connection available to ' + url + ', loading app.');
            return true
        } else {
            console.log('Connection available to ' + url + ' but has error status');
            return false
        }
    } catch (error) {
        console.log('No connection available to ' + url);
    }
}

async function systemAppBootstrap(url, interval) {
    const START_INTERVAL = 1000
    const INTERVAL_INCREASE = 1000
    const MAX_INTERVAL = 5000
    let ready = await checkUrlBeforeLoading(url)
    if (ready) {
        document.location.href = url
        return
    }

    if (interval) {
        interval += INTERVAL_INCREASE
        interval = Math.min(interval, MAX_INTERVAL)
    } else {
        interval = START_INTERVAL
    }
    console.log('Could not load app as it cannot be reached, trying again in ' + (interval / 1000) + 's')

    setTimeout(() => systemAppBootstrap(url, interval), interval)
}