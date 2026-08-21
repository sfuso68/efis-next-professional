void MAIN()
{
    vec4 satellite = texture(uColorMap, UV0);
    vec2 texel = 1.0 / vec2(textureSize(uColorMap, 0));
    vec3 neighbourhood = (
        texture(uColorMap, UV0 + vec2(texel.x, 0.0)).rgb +
        texture(uColorMap, UV0 - vec2(texel.x, 0.0)).rgb +
        texture(uColorMap, UV0 + vec2(0.0, texel.y)).rgb +
        texture(uColorMap, UV0 - vec2(0.0, texel.y)).rgb) * 0.25;
    vec3 detailed = clamp(satellite.rgb + (satellite.rgb - neighbourhood) * 0.62, 0.0, 1.0);
    detailed = clamp((detailed - 0.5) * 1.10 + 0.5, 0.0, 1.0);
    float clearance = uAircraftAltitudeM - VAR_WORLD_POSITION.y;
    float danger = 1.0 - smoothstep(30.0, 85.0, clearance);
    float warningEnvelope = 1.0 - smoothstep(300.0, 380.0, clearance);
    float warning = warningEnvelope * (1.0 - danger);
    vec3 warned = mix(detailed, vec3(0.97, 0.64, 0.08), warning * 0.30 * uTawsBlend);
    vec3 alerted = mix(warned, vec3(0.93, 0.08, 0.055), danger * 0.58 * uTawsBlend);
    BASE_COLOR = vec4(alerted, satellite.a);
    ROUGHNESS = 0.94;
    METALNESS = 0.0;
    SPECULAR_AMOUNT = 0.05;
}
