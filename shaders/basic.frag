#version 330 core

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;

out vec4 FragColor;

uniform vec4 uColor;
uniform vec4 uHighlightColor;
uniform bool uUseTexture;
uniform sampler2D uTexture;

uniform vec3 uLightPos;
uniform vec3 uViewPos;

void main()
{
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(uLightPos - FragPos);
    
    // Ambient
    float ambientStrength = 0.35;
    vec3 ambient = ambientStrength * vec3(1.0);
    
    // Diffuse
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * vec3(1.0);
    
    // Specular
    float specularStrength = 0.4;
    vec3 viewDir = normalize(uViewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular = specularStrength * spec * vec3(1.0);
    
    vec4 baseColor = uColor;
    if (uUseTexture) {
        baseColor *= texture(uTexture, TexCoord);
    }
    
    vec3 result = (ambient + diffuse + specular) * baseColor.rgb;
    
    // Add subtle highlight if selected
    result = mix(result, uHighlightColor.rgb, uHighlightColor.a);
    
    FragColor = vec4(result, baseColor.a);
}
