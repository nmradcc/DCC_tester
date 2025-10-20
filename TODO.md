1. Pin mapping screwed up when changing from F4 to H533 processor
2. DAC output pins do not map cleanly
3. Struct alignment issue with DCC library
4. Railcom cutout not tested
5. counter timer 14 delay for channel 1 bidi start (40us)
6. check for quite track voltage in decoder 
 we will use BR_ENABLE pin state for the time being 
 but should be replaced with proper no voltage on track detection ckt
 as we can not assume we will always be using our Command Station.
 i.e. We may be using decoder stand alone with external C.S.
