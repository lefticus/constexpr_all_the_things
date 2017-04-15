(function (window, undefined) {
    'use strict';

    window.addEventListener('load', function () {
        window.Reveal.addEventListener('slidechanged', function (event) {
            // event.previousSlide, event.currentSlide, event.indexh, event.indexv
        });
    });

})(window);
